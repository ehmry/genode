/*
 * \brief  Server::Entrypoint based NIC session component
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2015-06-22
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NIC__COMPONENT_H_
#define _INCLUDE__NIC__COMPONENT_H_

#include <base/attached_ram_dataspace.h>
#include <nic/packet_allocator.h>
#include <nic_session/rpc_object.h>
#include <util/xml_generator.h>
#include <base/log.h>

namespace Nic {

	class Communication_buffers;
	class Session_component;
	class State_component;
}


class Nic::Communication_buffers
{
	protected:

		Nic::Packet_allocator          _rx_packet_alloc;
		Genode::Attached_ram_dataspace _tx_ds, _rx_ds;

		Communication_buffers(Genode::Allocator &rx_block_md_alloc,
		                      Genode::Ram_session &ram,
		                      Genode::Region_map  &rm,
		                      Genode::size_t tx_size, Genode::size_t rx_size)
		:
			_rx_packet_alloc(&rx_block_md_alloc),
			_tx_ds(ram, rm, tx_size),
			_rx_ds(ram, rm, rx_size)
		{ }
};


class Nic::State_component : public Genode::Rpc_object<Genode::Rom_session>
{
	public:

		enum { MAX_IP_ADDR_LENGTH  = 16 };
		typedef Genode::String<MAX_IP_ADDR_LENGTH> Ipv4_addr;

	private:

		Ipv4_addr                         _addr,
		                                  _netmask,
		                                  _gateway;

		Genode::Attached_ram_dataspace    _ram_ds;
		Genode::Signal_context_capability _update_sigh;
		Mac_address                       _mac_addr;
		unsigned                          _mtu = 0;
		bool                              _link_state = false;
		bool                              _ready      = false;
		bool                              _pending    = true;

	public:

		State_component(Genode::Ram_session &ram,
		                Genode::Region_map  &rm,
		                Genode::size_t       size = 4096)
		: _ram_ds(ram, rm, size)
		{ }

		Mac_address mac_addr()                  const { return _mac_addr; }
		void        mac_addr(Mac_address const &addr)
		{
			_mac_addr = addr;
			_ready    = true;
		}

		bool link_state()     const { return  _link_state; }
		void link_state(bool state)
		{
			if (_link_state != state) {
				_link_state = state;
				_pending = true;
			}
		}

		void ipv4_addr   (Ipv4_addr const &a)
		{
			if (_addr != a) {
				_addr = a;
				_pending = true;
			}
		}
		void ipv4_netmask(Ipv4_addr const &a)
		{
			if (_netmask != a) {
				_netmask = a;
				_pending = true;
			}
		}
		void ipv4_gateway(Ipv4_addr const &a)
		{
			if (_gateway != a) {
				_gateway = a;
				_pending = true;
			}
		}

		Ipv4_addr ipv4_addr   () const { return _addr;    }
		Ipv4_addr ipv4_netmask() const { return _netmask; }
		Ipv4_addr ipv4_gateway() const { return _gateway; }

		unsigned mtu() const { return _mtu; }
		void mtu(unsigned m)
		{
			if (_mtu != m) {
				_mtu = m;
				_pending = true;
			}
		}

		void submit_signal()
		{
			if (_pending && _update_sigh.valid())
				Genode::Signal_transmitter(_update_sigh).submit();
		}


		/***************************
		 ** ROM session interface **
		 ***************************/

		Genode::Rom_dataspace_capability dataspace() override
		{
			using namespace Genode;
			if (!_ready)
				return Rom_dataspace_capability();
			update();
			Dataspace_capability ds_cap = static_cap_cast<Dataspace>(_ram_ds.cap());
			return static_cap_cast<Rom_dataspace>(ds_cap);
		}

		bool update() override
		{
			/* TODO: no need to update when nothing changes */

			if (!(_ready && _pending)) {
				return false;
			} else {
				char mac_str[18];
				Genode::snprintf(mac_str, sizeof(mac_str),
				                 "%02x:%02x:%02x:%02x:%02x:%02x",
				                 _mac_addr.addr[0] & 0xff, _mac_addr.addr[1] & 0xff,
				                 _mac_addr.addr[2] & 0xff, _mac_addr.addr[3] & 0xff,
				                 _mac_addr.addr[4] & 0xff, _mac_addr.addr[5] & 0xff);

				Genode::Xml_generator gen(_ram_ds.local_addr<char>(),
				                          _ram_ds.size(), "nic", [&] () {

					gen.attribute("link_state", _link_state);
					gen.attribute("mac_addr", mac_str);
					if (_mtu) gen.attribute("mtu", _mtu);

					if ((_addr != "") || (_netmask != "")) gen.node("ipv4", [&] () {
						if (_addr != "")
							gen.attribute("addr", _addr);
						if (_netmask != "")
							gen.attribute("netmask", _netmask);
						if (_gateway != "")
							gen.attribute("gateway", _gateway);
					});
				});
				return true;
			}
		}

		void sigh(Genode::Signal_context_capability cap) override {
			_update_sigh = cap; }
};


class Nic::Session_component : Communication_buffers, public Session_rpc_object
{
	protected:

		/**
		 * Sub-classes must implement this function, it is called upon all
		 * packet-stream signals.
		 */
		virtual void _handle_packet_stream() = 0;

		Genode::Signal_handler<Session_component> _packet_stream_handler;

	public:

		/**
		 * Constructor
		 *
		 * \param tx_buf_size        buffer size for tx channel
		 * \param rx_buf_size        buffer size for rx channel
		 * \param rx_block_md_alloc  backing store of the meta data of the
		 *                           rx block allocator
		 * \param ram                RAM session to allocate tx and rx buffers
		 * \param rm                 Region to map stream buffers into
		 * \param ep                 entry point used for packet stream
		 *                           channels
		 */
		Session_component(Genode::size_t const tx_buf_size,
		                  Genode::size_t const rx_buf_size,
		                  Genode::Allocator   &rx_block_md_alloc,
		                  Genode::Ram_session &ram,
		                  Genode::Region_map  &rm,
		                  Genode::Entrypoint  &ep)
		:
			Communication_buffers(rx_block_md_alloc, ram, rm,
			                      tx_buf_size, rx_buf_size),
			Session_rpc_object(_tx_ds.cap(), _rx_ds.cap(), rm,
			                   _rx_packet_alloc, ep.rpc_ep()),
			_packet_stream_handler(ep, *this, &Session_component::_handle_packet_stream)
		{
			/* install data-flow signal handlers for both packet streams */
			_tx.sigh_ready_to_ack   (_packet_stream_handler);
			_tx.sigh_packet_avail   (_packet_stream_handler);
			_rx.sigh_ready_to_submit(_packet_stream_handler);
			_rx.sigh_ack_avail      (_packet_stream_handler);
		}
};


#endif /* _INCLUDE__NIC__COMPONENT_H_ */
