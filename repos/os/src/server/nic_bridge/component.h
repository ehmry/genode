/*
 * \brief  Proxy-ARP session and root component
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _COMPONENT_H_
#define _COMPONENT_H_

/* Genode includes */
#include <base/log.h>
#include <base/heap.h>
#include <base/ram_allocator.h>
#include <nic/packet_allocator.h>
#include <nic_session/rpc_object.h>
#include <nic_session/connection.h>
#include <os/session_policy.h>
#include <root/component.h>
#include <util/arg_string.h>

/* NIC router includes */
#include <mac_allocator.h>

/* local includes */
#include <address_node.h>
#include <uplink.h>
#include <packet_handler.h>

namespace Net {
	class Stream_allocator;
	class Stream_dataspace;
	class Stream_dataspaces;
	class Session_component;

	typedef Genode::Id_space<Session_component> Session_space;
}


/**
 * Nic-session component class
 *
 * We must inherit here from Stream_allocator, although aggregation
 * would be more convinient, because the allocator needs to be initialized
 * before base-class Session_rpc_object.
 */
class Net::Session_component : private ::Net::Session_resources,
                               public  Nic::Session_rpc_object,
                               public  Net::Packet_handler
{
	private:

		Session_space::Element            _sessions_elem;
		Mac_address_node                  _mac_node;
		Ipv4_address_node                 _ipv4_node;
		Net::Uplink                      &_uplink;
		Genode::Signal_context_capability _link_state_sigh { };

		void _unset_ipv4_node();

	public:

		typedef Genode::String<32> Ip_addr;

		/**
		 * Constructor
		 *
		 * \param ram          ram session
		 * \param rm           region map of this component
		 * \param ep           entry point this session component is handled by
		 * \param amount       amount of memory managed by guarded allocator
		 * \param tx_buf_size  buffer size for tx channel
		 * \param rx_buf_size  buffer size for rx channel
		 * \param vmac         virtual mac address
		 */
		Session_component(Session_space &space,
		                  Session_space::Id id,
		                  Genode::Ram_allocator       &ram,
		                  Genode::Region_map          &rm,
		                  Genode::Entrypoint          &ep,
		                  Genode::Ram_quota            ram_quota,
		                  Genode::Cap_quota            cap_quota,
		                  Nic::Session::Tx_size        tx_buf_size,
		                  Nic::Session::Rx_size        rx_buf_size,
		                  Mac_address                  vmac,
		                  Net::Uplink                 &uplink,
		                  bool                  const &verbose,
		                  Genode::Session_label const &label,
		                  Ip_addr               const &ip_addr);

		~Session_component();

		::Nic::Mac_address mac_address() override
		{
			::Nic::Mac_address m;
			Mac_address_node::Address mac = _mac_node.addr();
			Genode::memcpy(&m, mac.addr, sizeof(m.addr));
			return m;
		}

		void link_state_changed()
		{
			if (_link_state_sigh.valid())
				Genode::Signal_transmitter(_link_state_sigh).submit();
		}

		void set_ipv4_address(Ipv4_address ip_addr);


		/****************************************
		 ** Nic::Driver notification interface **
		 ****************************************/

		void link_state(Link_state) override { }

		Link_state session_link_state() override {
			return _uplink.link_state(); }

		void link_state_sigh(Genode::Signal_context_capability sigh) override {
			_link_state_sigh = sigh; }


		/******************************
		 ** Packet_handler interface **
		 ******************************/

		Packet_stream_sink< ::Nic::Session::Policy> * sink() override {
			return _tx.sink(); }

		Packet_stream_source< ::Nic::Session::Policy> * source() override {
			return _rx.source(); }


		bool handle_arp(Ethernet_frame &eth,
		                Size_guard     &size_guard) override;

		bool handle_ip(Ethernet_frame &eth,
		               Size_guard     &size_guard) override;

		void finalize_packet(Ethernet_frame *, Genode::size_t) override;

		Mac_address vmac() const { return _mac_node.addr(); }
};

#endif /* _COMPONENT_H_ */
