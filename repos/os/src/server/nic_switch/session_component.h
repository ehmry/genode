#include <base/debug.h>
/*
 * \brief  Nic switch session component
 * \author Emery Hemingway
 * \date   2019-04-10
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SESSION_COMPONENT_H_
#define _SESSION_COMPONENT_H_

/* local includes */
#include "network.h"

/* Genode includes */
#include <net/ethernet.h>
#include <net/size_guard.h>
#include <nic/component.h>
#include <base/heap.h>
#include <base/registry.h>
#include <base/session_label.h>

namespace Nic_switch {
	using namespace Net;

	struct Tx_size { Genode::size_t value; };
	struct Rx_size { Genode::size_t value; };

	class Session_resources;
	class Session_component;

	typedef Genode::Registry<Session_component> Interface_registry;
}

/**
 * Base class to manage session quotas and allocations
 */
class Nic_switch::Session_resources
{
	protected:

		Genode::Ram_quota_guard           _ram_guard;
		Genode::Cap_quota_guard           _cap_guard;
		Genode::Constrained_ram_allocator _ram_alloc;
		Genode::Attached_ram_dataspace    _tx_ds, _rx_ds;
		Genode::Heap                      _alloc;
		Nic::Packet_allocator             _rx_pkt_alloc { &_alloc };

		Session_resources(Genode::Ram_allocator &ram,
		                  Genode::Region_map    &region_map,
		                  Genode::Ram_quota     ram_quota,
		                  Genode::Cap_quota     cap_quota,
		                  Tx_size         tx_size,
		                  Rx_size         rx_size)
		:
			_ram_guard(ram_quota), _cap_guard(cap_quota),
			_ram_alloc(ram, _ram_guard, _cap_guard),
			_tx_ds(_ram_alloc, region_map, tx_size.value),
			_rx_ds(_ram_alloc, region_map, rx_size.value),
			_alloc(_ram_alloc, region_map)
		{ }
};


class Nic_switch::Session_component : private Session_resources,
                                      public Nic::Session_rpc_object
{
	private:

		Network            &_network;
		Interface_registry &_interface_registry;

		Interface_registry::Element _interface_element
			{ _interface_registry, *this };

		Genode::Io_signal_handler<Session_component> _packet_handler;

		Genode::Session_label const _label;

		Mac_address const _client_mac { _network.mac_alloc.alloc() };
		Mac_address const  _local_mac { _network.mac_alloc.alloc() };

		Nic::Packet_stream_sink<::Nic::Session::Policy> &sink() {
			return *_tx.sink(); }

		Nic::Packet_stream_source<::Nic::Session::Policy> &source() {
			return *_rx.source(); }

		void _send(Ethernet_frame &eth, Genode::size_t const size)
		{
			while (source().ack_avail())
				source().release_packet(source().get_acked_packet());

			Genode::log(_label, " rcv ", eth);

			Nic::Packet_descriptor packet = source().alloc_packet(size);
			void *content = source().packet_content(packet);
			Genode::memcpy(content, (void*)&eth, size);
			source().submit_packet(packet);
		}

		void _handle_ipv6(Ethernet_frame &eth,
		                  Size_guard &size_guard,
		                  Genode::size_t const size)
		{
			(void)size;
			Ipv6_packet &ip = eth.data<Ipv6_packet>(size_guard);

			/* 4.1
			 * if the packet is ipv6, check the neighbor cache
			 * for the source address and create a STALE entry if
			 * it was not found.
			 *
			 * if the packet is ipv6 and ICMP ND, the ICMP must be
			 * proxied, that is, the link-layer addresses within
			 * the ICMP payload must be modified
			 *
			 * if the packet is multicast then forward the packet
			 * out all interfaces on the same link with a new
			 * link-layer header
			 *
			 * when any other IPV6 packet is recieved it is forwarded
			 * with a new link-layer header to the interface associated
			 * with the next hop in the neighbor cache. if no entry
			 * exists, drop the packet
			 */

			/* 4.1.3.
			 * ICMP ND packets must update the neighbor cache
			 */

			if (!ip.src().unspecified()) {
				auto hit = [&] (Neighbor &) { };
				auto miss = [&] (Neighbor_cache::Missing_element &missing) {
					auto mac = eth.src();
					auto state = Neighbor::STALE;
					Genode::log("create STALE neighbor ", ip.src());
					missing.construct(*this, mac, state);
				};
				_network.neighbors.try_apply(ip.src(), hit, miss);
			}

			if (ip.protocol() == Ip_protocol::ICMP6) {
				Genode::warning("replace ICMPv6 link-layer addresses");
			}
		}

		void _handle_ethernet(Ethernet_frame &eth,
		                      Size_guard &size_guard,
		                      Genode::size_t const size)
		{
			switch (eth.type()) {
			case Ethernet_frame::Type::IPV6:
				_handle_ipv6(eth, size_guard, size); break;
			default:
				/* drop packet */
				/* send it out anyway */
				_interface_registry.for_each([&] (Session_component &other) {
					if (&other != this) other._send(eth, size);
				});
				break;
			}
		}

		bool _handle_packet(Nic::Packet_descriptor const &pkt)
		{
			if (!pkt.size() || !sink().packet_valid(pkt)) return true;

			Size_guard size_guard(pkt.size());
			Ethernet_frame &eth = Ethernet_frame::cast_from(
				sink().packet_content(pkt), size_guard);

			Genode::log(_label, " snd ", eth);

			_handle_ethernet(eth, size_guard,pkt.size());

			return true;
		}

		void _handle_packets()
		{
			while (sink().ready_to_ack() && sink().packet_avail()) {
				if (_handle_packet(sink().peek_packet()))
					sink().acknowledge_packet(sink().get_packet());
				else
					break;
			}
		}

	public:

		Session_component(Genode::Entrypoint    &ep,
		                  Genode::Ram_allocator &ram,
		                  Genode::Region_map    &region_map,
		                  Genode::Ram_quota      ram_quota,
		                  Genode::Cap_quota      cap_quota,
		                  Tx_size                tx_size,
		                  Rx_size                rx_size,
		                  Network               &network,
		                  Interface_registry    &interface_registry,
		                  Genode::Session_label const &label)
		:
			Session_resources(ram, region_map,
			                  ram_quota, cap_quota,
			                  tx_size, rx_size),
			Nic::Session_rpc_object(region_map,
			                        _tx_ds.cap(), _rx_ds.cap(),
			                        &_rx_pkt_alloc, ep.rpc_ep()),
			_network(network),
			_interface_registry(interface_registry),
			_packet_handler(ep, *this, &Session_component::_handle_packets),
			_label(label)
		{
			_tx.sigh_packet_avail(_packet_handler);
			_tx.sigh_ready_to_ack(_packet_handler);
		}

		Nic::Mac_address mac_address() override { return _client_mac; }

		bool link_state() override { return true; }

		void link_state_sigh(Genode::Signal_context_capability) override { }

};

#endif /* _SESSION_COMPONENT_H_ */
