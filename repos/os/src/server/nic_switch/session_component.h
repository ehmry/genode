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

		Interface_registry &_interface_registry;

		Interface_registry::Element _interface_element
			{ _interface_registry, *this };

		Genode::Io_signal_handler<Session_component> _packet_handler;

		Genode::Session_label const _label;
		Nic::Mac_address      const _mac;

		Nic::Packet_stream_sink<::Nic::Session::Policy> &sink() {
			return *_tx.sink(); }

		Nic::Packet_stream_source<::Nic::Session::Policy> &source() {
			return *_rx.source(); }

		void _send(Ethernet_frame &eth, Genode::size_t const size)
		{
			Genode::log(_label, " rcv ", eth);

			Nic::Packet_descriptor packet = source().alloc_packet(size);
			void *content = source().packet_content(packet);
			Genode::memcpy(content, (void*)&eth, size);
			source().submit_packet(packet);
		}

		void _handle_ethernet(Ethernet_frame &eth,
		                      Size_guard &size_guard,
		                      Genode::size_t const size)
		{
			(void)size_guard;
			_interface_registry.for_each([&] (Session_component &other) {
				if (&other != this) other._send(eth, size);
			});
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
		                  Interface_registry    &interface_registry,
		                  Genode::Session_label const &label,
		                  Nic::Mac_address       mac)
		:
			Session_resources(ram, region_map,
			                  ram_quota, cap_quota,
			                  tx_size, rx_size),
			Nic::Session_rpc_object(region_map,
			                        _tx_ds.cap(), _rx_ds.cap(),
			                        &_rx_pkt_alloc, ep.rpc_ep()),
			_interface_registry(interface_registry),
			_packet_handler(ep, *this, &Session_component::_handle_packets),
			_label(label), _mac(mac)
		{
			_tx.sigh_packet_avail(_packet_handler);
			_tx.sigh_ready_to_ack(_packet_handler);
		}

		Nic::Mac_address mac_address() override { return _mac; }

		bool link_state() override { return true; }

		void link_state_sigh(Genode::Signal_context_capability) override { }

};

#endif /* _SESSION_COMPONENT_H_ */
