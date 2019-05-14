/*
 * \brief  Glue code for Linux network drivers
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2012-07-05
 */

/*
 * Copyright (C) 2012-2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>
#include <nic/xml_node.h>
#include <base/log.h>
#include <base/snprintf.h>
#include <base/registry.h>
#include <base/tslab.h>
#include <util/xml_node.h>

/* local includes */
#include <lx.h>
#include <lx_emul.h>

#include <lx_kit/env.h>

#include <lx_emul/extern_c_begin.h>
# include <linux/skbuff.h>
# include <net/cfg80211.h>
# include <lxc.h>
#include <lx_emul/extern_c_end.h>


enum {
	HEAD_ROOM = 128, /* XXX guessed value but works */
};


struct Tx_data
{
	net_device     *ndev;
	struct sk_buff *skb;
	Lx::Task       *task;
	int             err;
};

static Lx::Task *_tx_task;
static Tx_data   _tx_data;


static void _run_tx_task(void *args)
{
	Tx_data *data = static_cast<Tx_data*>(args);

	while (1) {
		Lx::scheduler().current()->block_and_schedule();

		net_device    *ndev = data->ndev;
		struct sk_buff *skb = data->skb;

		data->err = ndev->netdev_ops->ndo_start_xmit(skb, ndev);
		if (data->err) {
			Genode::warning("xmit failed: ", data->err, " skb: ", skb);
		}

		data->skb = nullptr;

		if (data->task) {
			data->task->unblock();
			data->task = nullptr;
		}
	}
}


bool tx_task_send(struct sk_buff *skb)
{
	if (_tx_data.skb) {
		Genode::error("skb: ", skb, " already queued");
		return false;
	}

	if (!_tx_task) {
		Genode::error("no TX task available");
		return false;
	}

	_tx_data.ndev = skb->dev;
	_tx_data.skb  = skb;
	_tx_data.task = Lx::scheduler().current();

	_tx_task->unblock();
	Lx::scheduler().current()->block_and_schedule();

	return true;
}


/**
 * Nic::Session implementation
 */
class Wifi_nic_proxy
{
	public:

		static Wifi_nic_proxy *instance;
		net_device            &device;

	private:

		Nic::Session::Link_state _device_link_state() const
		{
			return device.state & __LINK_STATE_NOCARRIER
				? Nic::Session::LINK_DOWN : Nic::Session::LINK_UP;
		}

		Nic::Mac_address _mac() const
		{
			Nic::Mac_address m;
			Genode::memcpy(&m, device.perm_addr, ETH_ALEN);
			return m;
		}

		Nic::Packet_allocator _nic_tx_alloc;
		Nic::Connection       _nic;

		Genode::Signal_handler<Wifi_nic_proxy>    _nic_link_handler;
		Genode::Io_signal_handler<Wifi_nic_proxy> _nic_packets_handler;

		Nic::Session::Link_state _link_state { Nic::Session::LINK_DOWN };

		void _handle_nic_link()
		{
			if ((_nic.session_link_state() == Nic::Session::LINK_UP)
			 && (_device_link_state() == Nic::Session::LINK_UP))
			{
				_link_state = Nic::Session::LINK_UP;
			} else {
				_link_state = Nic::Session::LINK_DOWN;
			}
		}

		bool _send()
		{
			using namespace Genode;
			auto &sink = *_nic.rx();

			/*
			 * We must not be called from another task, just from the
			 * packet stream dispatcher.
			 */
			if (Lx::scheduler().active()) {
				warning("scheduler active");
				return false;
			}

			if (_link_state == Nic::Session::LINK_DOWN) {
				Packet_descriptor packet = sink.get_packet();
				sink.acknowledge_packet(packet);
				Genode::warning("no link, drop packet");
				return true;
			}

			if (!sink.ready_to_ack() || !sink.packet_avail()) { return false; }

			Packet_descriptor const packet = sink.get_packet();
			if (!packet.size() || !sink.packet_valid(packet)) {
				warning("invalid packet from Nic session");
				return true;
			}

			struct sk_buff *skb = lxc_alloc_skb(packet.size() + HEAD_ROOM, HEAD_ROOM);

			skb->dev = &device;

			unsigned char *data = lxc_skb_put(skb, packet.size());
			Genode::memcpy(data, sink.packet_content(packet), packet.size());

			_tx_data.ndev = &device;
			_tx_data.skb  = skb;

			_tx_task->unblock();
			Lx::scheduler().schedule();

			sink.acknowledge_packet(packet);

			return true;
		}

		void _handle_nic_packets() { while (_send()) { continue; } }

	public:

		Wifi_nic_proxy(Genode::Env  &env,
		               Genode::Allocator &alloc,
		               net_device &ndev)
		: device(ndev),
		  _nic_tx_alloc(&alloc),
		  _nic(env, _mac(), _nic_tx_alloc,
		       Nic::Connection::default_tx_size(),
		       Nic::Connection::default_rx_size(),
		       "wifi"),
		  _nic_link_handler(env.ep(), *this,
		                    &Wifi_nic_proxy::_handle_nic_link),
		  _nic_packets_handler(env.ep(), *this,
		                       &Wifi_nic_proxy::_handle_nic_packets)
		{
			device.lx_nic_device = this;

			/* set Nic session signal handlers */
			_nic.link_state_sigh(_nic_link_handler);
			_nic.rx_channel()->sigh_packet_avail(_nic_packets_handler);
			_nic.rx_channel()->sigh_ready_to_ack(_nic_packets_handler);
		}

		~Wifi_nic_proxy()
		{
			device.lx_nic_device = nullptr;
		}

		/**
		 * Report link state to Nic server
		 */
		void update_link_state()
		{
			if (_link_state != _device_link_state())
				_nic.link_state(_device_link_state());
		}

		void receive(struct sk_buff *skb)
		{
			auto &source = *_nic.tx();

			/* flush acknowledgements */
			while (source.ack_avail())
				source.release_packet(source.get_acked_packet());

			if ((_link_state == Nic::Session::LINK_DOWN)
			 || (!source.ready_to_submit()))
			{
				Genode::warning("not ready to receive packet");
				return;
			}

			Skb s = skb_helper(skb);
			try {
				Nic::Packet_descriptor p = source.alloc_packet(s.packet_size + s.frag_size);
				void *buffer = source.packet_content(p);
				memcpy(buffer, s.packet, s.packet_size);

				if (s.frag_size)
					memcpy((char *)buffer + s.packet_size, s.frag, s.frag_size);

				source.submit_packet(p);
			} catch (...) {
				Genode::warning("failed to process received packet");
			}
		}
};


static Genode::Env       *_proxy_env   { nullptr };
static Genode::Allocator *_proxy_alloc { nullptr };


Wifi_nic_proxy *Wifi_nic_proxy::instance;


void Lx::nic_init(Genode::Env &env, Genode::Allocator &alloc)
{
	_proxy_env = &env;
	_proxy_alloc = &alloc;
}


void Lx::get_mac_address(unsigned char *addr)
{
	memcpy(addr, Wifi_nic_proxy::instance->device.perm_addr, ETH_ALEN);
}


namespace Lx {
	class Notifier;
}


class Lx::Notifier
{
	private:

		struct Block : public Genode::List<Block>::Element
		{
			struct notifier_block *nb;

			Block(struct notifier_block *nb) : nb(nb) { }
		};

		Lx_kit::List<Block> _list;
		Genode::Tslab<Block, 32 * sizeof(Block)> _block_alloc;

		void *_ptr;

	public:

		Notifier(Genode::Allocator &alloc, void *ptr)
		: _block_alloc(&alloc), _ptr(ptr) { }

		virtual ~Notifier() { };

		bool handles(void *ptr)
		{
			return _ptr == ptr;
		}

		void register_block(struct notifier_block *nb)
		{
			Block *b = new (&_block_alloc) Block(nb);
			_list.insert(b);
		}

		void unregister_block(struct notifier_block *nb)
		{
			for (Block *b = _list.first(); b; b = b->next())
				if (b->nb == nb) {
					_list.remove(b);
					destroy(&_block_alloc, b);
					break;
				}
		}

		int call_all_blocks(unsigned long val, void *v)
		{
			int rv = NOTIFY_DONE;
			for (Block *b = _list.first(); b; b = b->next()) {
				rv = b->nb->notifier_call(b->nb, val, v);
				if (rv & NOTIFY_STOP_MASK)
					break;
			}
			return rv;
		}
};


static Genode::Registry<Genode::Registered<Lx::Notifier>> _blocking_notifier_registry;


/* XXX move blocking_notifier_call to proper location */
/**********************
 ** linux/notifier.h **
 **********************/

static Lx::Notifier &blocking_notifier(struct blocking_notifier_head *nh)
{
	Lx::Notifier *notifier = nullptr;
	auto lookup = [&](Lx::Notifier &n) {
		if (!n.handles(nh)) { return; }

		notifier = &n;
	};
	_blocking_notifier_registry.for_each(lookup);

	if (!notifier) {
		Genode::Registered<Lx::Notifier> *n = new (&Lx_kit::env().heap())
			Genode::Registered<Lx::Notifier>(_blocking_notifier_registry,
			                                 Lx_kit::env().heap(), nh);
		notifier = &*n;
	}

	return *notifier;
}


int blocking_notifier_chain_register(struct blocking_notifier_head *nh,
                                          struct notifier_block *nb)
{
	blocking_notifier(nh).register_block(nb);
	return 0;
}

int blocking_notifier_chain_unregister(struct blocking_notifier_head *nh,
                                          struct notifier_block *nb)
{
	blocking_notifier(nh).unregister_block(nb);
	return 0;
}


int blocking_notifier_call_chain(struct blocking_notifier_head *nh,
                                 unsigned long val, void *v)
{
	return blocking_notifier(nh).call_all_blocks(val, v);
}


/***********************
 ** linux/netdevice.h **
 ***********************/

static Lx::Notifier &net_notifier()
{
	static Lx::Notifier inst(Lx_kit::env().heap(), NULL);
	return inst;
}


extern "C" int register_netdevice_notifier(struct notifier_block *nb)
{
	/**
	 * In Linux the nb is actually called upon on registration. We do not
	 * that semantic because we add a net_device only after all notifiers
	 * were registered.
	 */
	net_notifier().register_block(nb);

	return 0;
}


extern "C" int unregster_netdevice_notifier(struct notifier_block *nb)
{
	net_notifier().unregister_block(nb);

	return 0;
}


extern "C" struct net_device * netdev_notifier_info_to_dev(struct netdev_notifier_info *info)
{
	/* we always pass a net_device pointer to this function */
	return reinterpret_cast<net_device *>(info);
}


struct Proto_hook : public Lx_kit::List<Proto_hook>::Element
{
	struct packet_type &pt;

	Proto_hook(struct packet_type *pt) : pt(*pt) { }
};


class Proto_hook_list
{
	private:

		Lx_kit::List<Proto_hook>  _list;
		Genode::Allocator    &_alloc;

	public:

		Proto_hook_list(Genode::Allocator &alloc) : _alloc(alloc) { }

		void insert(struct packet_type *pt) {
			_list.insert(new (&_alloc) Proto_hook(pt)); }

		void remove(struct packet_type *pt)
		{
			for (Proto_hook *ph = _list.first(); ph; ph = ph->next())
				if (&ph->pt == pt) {
					_list.remove(ph);
					destroy(&_alloc, ph);
					break;
				}
		}

		Proto_hook* first() { return _list.first(); }
};


static Proto_hook_list& proto_hook_list()
{
	static Proto_hook_list inst(Lx_kit::env().heap());
	return inst;
}


extern "C" void dev_add_pack(struct packet_type *pt)
{
	proto_hook_list().insert(pt);
}


extern "C" void __dev_remove_pack(struct packet_type *pt)
{
	proto_hook_list().remove(pt);
}


extern "C" struct net_device *__dev_get_by_index(struct net *net, int ifindex)
{
	if (!Wifi_nic_proxy::instance) {
		Genode::error("no net device registered!");
		return 0;
	}

	return &Wifi_nic_proxy::instance->device;
}


extern "C" struct net_device *dev_get_by_index_rcu(struct net *net, int ifindex)
{
	return __dev_get_by_index(net, ifindex);
}


extern "C" struct net_device *dev_get_by_index(struct net *net, int ifindex)
{
	return __dev_get_by_index(net, ifindex);
}


extern "C" int dev_hard_header(struct sk_buff *skb, struct net_device *dev,
                               unsigned short type, const void *daddr,
                               const void *saddr, unsigned int len)
{
	if (!dev->header_ops || !dev->header_ops->create)
		return 0;

	return dev->header_ops->create(skb, dev, type, daddr, saddr, len);
}


extern "C" int dev_parse_header(const struct sk_buff *skb, unsigned char *haddr)
{
	struct net_device const *dev = skb->dev;

	if (!dev->header_ops || dev->header_ops->parse)
		return 0;

	return dev->header_ops->parse(skb, haddr);
}


extern "C" int dev_queue_xmit(struct sk_buff *skb)
{
	if (skb->next)
		Genode::warning("more skb's queued");
	return tx_task_send(skb) ? NETDEV_TX_OK : -1;
}


extern "C" size_t LL_RESERVED_SPACE(struct net_device *dev)
{
	return dev->hard_header_len ?
	       (dev->hard_header_len + (HH_DATA_MOD - 1)) & ~(HH_DATA_MOD - 1) : 0;
}


extern "C" void dev_close(struct net_device *ndev)
{
	/*
	 * First instruct cfg80211 to leave the associated network
	 * and then shutdown the interface.
	 */
	net_notifier().call_all_blocks(NETDEV_GOING_DOWN, ndev);
	net_notifier().call_all_blocks(NETDEV_DOWN, ndev);

	ndev->state &= ~(1UL << __LINK_STATE_START);
	netif_carrier_off(ndev);

	const struct net_device_ops *ops = ndev->netdev_ops;
	if (ops->ndo_stop) { ops->ndo_stop(ndev); }

	ndev->flags &= ~IFF_UP;
}


bool Lx::open_device()
{
	if (!Wifi_nic_proxy::instance) {
		Genode::error("no net_device available");
		return false;
	}

	struct net_device * const ndev = &Wifi_nic_proxy::instance->device;

	int err = ndev->netdev_ops->ndo_open(ndev);
	if (err) {
		Genode::error("Open device failed");
		throw -1;
		return err;
	}

	/*
	 * Important, otherwise netif_running checks fail and AF_PACKET
	 * will not bind and EAPOL will cease to work.
	 */
	ndev->flags |= IFF_UP;
	ndev->state |= (1UL << __LINK_STATE_START);

	if (ndev->netdev_ops->ndo_set_rx_mode)
		ndev->netdev_ops->ndo_set_rx_mode(ndev);

	net_notifier().call_all_blocks(NETDEV_UP, ndev);

	return true;
}


extern "C" int register_netdevice(struct net_device *ndev)
{
	if (Wifi_nic_proxy::instance) {
		Genode::error("We don't support multiple network devices in one driver instance");
		return -ENODEV;
	}

	static Wifi_nic_proxy proxy(*_proxy_env, *_proxy_alloc, *ndev);

	ndev->state |= 1UL << __LINK_STATE_START;
	netif_carrier_off(ndev);

	/* execute all notifier blocks */
	net_notifier().call_all_blocks(NETDEV_REGISTER, ndev);
	net_notifier().call_all_blocks(NETDEV_UP, ndev);
	ndev->ifindex = 1;

	/* set mac adress */
	Genode::memcpy(ndev->perm_addr, ndev->ieee80211_ptr->wiphy->perm_addr, ETH_ALEN);

	{
		using namespace Genode;
		log("mac_address: ",
		    Hex(ndev->perm_addr[0], Hex::OMIT_PREFIX, Hex::PAD), ":",
		    Hex(ndev->perm_addr[1], Hex::OMIT_PREFIX, Hex::PAD), ":",
		    Hex(ndev->perm_addr[2], Hex::OMIT_PREFIX, Hex::PAD), ":",
		    Hex(ndev->perm_addr[3], Hex::OMIT_PREFIX, Hex::PAD), ":",
		    Hex(ndev->perm_addr[4], Hex::OMIT_PREFIX, Hex::PAD), ":",
		    Hex(ndev->perm_addr[5], Hex::OMIT_PREFIX, Hex::PAD));
	}

	int err = ndev->netdev_ops->ndo_open(ndev);
	if (err) {
		Genode::error("Initializing device failed");
		throw -1;
		return err;
	}

	static Lx::Task tx_task { _run_tx_task, &_tx_data, "tx_task",
	                          Lx::Task::PRIORITY_1, Lx::scheduler() };
	_tx_task = &tx_task;

	if (ndev->netdev_ops->ndo_set_rx_mode)
		ndev->netdev_ops->ndo_set_rx_mode(ndev);

	Wifi_nic_proxy::instance->update_link_state();

	list_add_tail_rcu(&ndev->dev_list, &init_net.dev_base_head);

	return 0;
}


extern "C" int netif_running(const struct net_device *dev)
{
	return dev->state & (1UL << __LINK_STATE_START);
}


extern "C" int netif_device_present(struct net_device *dev) { return 1; }


extern "C" int netif_carrier_ok(const struct net_device *dev)
{
	return !(dev->state & (1UL << __LINK_STATE_NOCARRIER));
}


extern "C" void netif_carrier_on(struct net_device *dev)
{
	dev->state &= ~(1UL << __LINK_STATE_NOCARRIER);

	Wifi_nic_proxy *proxy = (Wifi_nic_proxy *)dev->lx_nic_device;

	if (proxy)
		proxy->update_link_state();
}


extern "C" void netif_carrier_off(struct net_device *dev)
{
	dev->state |= 1UL << __LINK_STATE_NOCARRIER;

	Wifi_nic_proxy *proxy = (Wifi_nic_proxy *)dev->lx_nic_device;

	if (proxy)
		proxy->update_link_state();
}


extern "C" int netif_receive_skb(struct sk_buff *skb)
{
	/**
	 * XXX check original linux implementation if it is really
	 * necessary to free the skb if it was not handled.
	 */

	/* send EAPOL related frames only to the wpa_supplicant */
	if (is_eapol(skb)) {
		/* XXX call only AF_PACKET hook */
		for (Proto_hook* ph = proto_hook_list().first(); ph; ph = ph->next()) {
			ph->pt.func(skb, &Wifi_nic_proxy::instance->device,
			            &ph->pt, &Wifi_nic_proxy::instance->device);
		}
		return NET_RX_SUCCESS;
	}

	if (Wifi_nic_proxy::instance)
		Wifi_nic_proxy::instance->receive(skb);

	dev_kfree_skb(skb);
	return NET_RX_SUCCESS;
}


gro_result_t napi_gro_receive(struct napi_struct *napi, struct sk_buff *skb)
{
	return netif_receive_skb(skb);
}


extern "C" void netif_start_subqueue(struct net_device *dev, u16 queue_index)
{
	dev->_tx[queue_index].state = NETDEV_QUEUE_START;
}


extern "C" void netif_stop_subqueue(struct net_device *dev, u16 queue_index)
{
	dev->_tx[queue_index].state = 0;
}


extern "C" void netif_wake_subqueue(struct net_device *dev, u16 queue_index)
{
	dev->_tx[queue_index].state = NETDEV_QUEUE_START;
}


extern "C" u16 netdev_cap_txqueue(struct net_device *dev, u16 queue_index)
{
	if (queue_index > dev-> real_num_tx_queues) {
		Genode::error("queue_index ", queue_index, " out of range (",
		              dev->real_num_tx_queues, " max)");
		return 0;
	}

	return queue_index;
}


extern "C" struct net_device *alloc_netdev_mqs(int sizeof_priv, const char *name,
                                    unsigned char name_assign_type,
                                    void (*setup)(struct net_device *),
                                    unsigned int txqs, unsigned int rxqs)
{
	struct net_device *dev;
	size_t alloc_size;
	struct net_device *p;

	alloc_size = ALIGN(sizeof(struct net_device), NETDEV_ALIGN);
	/* ensure 32-byte alignment of whole construct */
	alloc_size += NETDEV_ALIGN - 1;

	p = (net_device *)kzalloc(alloc_size, GFP_KERNEL);
	if (!p)
		return NULL;

	dev = PTR_ALIGN(p, NETDEV_ALIGN);

	dev->gso_max_size = GSO_MAX_SIZE;
	dev->gso_max_segs = GSO_MAX_SEGS;

	setup(dev);

	/* actually set by dev_open() */
	dev->flags |= IFF_UP;


	/* XXX our dev is always called wlan0 */
	strcpy(dev->name, "wlan0");

	dev->dev_addr = (unsigned char *)kzalloc(ETH_ALEN, GFP_KERNEL);
	if (!dev->dev_addr)
		return 0;

	if (sizeof_priv) {
		/* ensure 32-byte alignment of private area */
		dev->priv = kzalloc(sizeof_priv, GFP_KERNEL);
		if (!dev->priv)
			return 0;
	}

	dev->num_tx_queues = txqs;
	dev->real_num_tx_queues = txqs;
	struct netdev_queue *tx = (struct netdev_queue*)
	                           kcalloc(txqs, sizeof(struct netdev_queue),
	                                   GFP_KERNEL | GFP_LX_DMA);
	if (!tx) {
		Genode::error("could not allocate ndev_queues");
	}

	dev->_tx = tx;
	for (unsigned i = 0; i < txqs; i++) {
		tx[i].dev = dev;
		tx[i].numa_node = NUMA_NO_NODE;
	}

	return dev;
}


/*************************
 ** linux/etherdevice.h **
 *************************/

int is_valid_ether_addr(const u8 *addr)
{
	/* is multicast */
	if ((addr[0] & 0x1))
		return 0;

	/* zero */
	if (!(addr[0] | addr[1] | addr[2] | addr[3] | addr[4] | addr[5]))
		return 0;

	return 1;
}
