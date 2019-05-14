/*
 * \brief  Proxy-ARP uplink session
 * \author Emery Hemingway
 * \date   2019-05-14
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__SERVER__NIC_BRIDGE__UPLINK_H_
#define _SRC__SERVER__NIC_BRIDGE__UPLINK_H_

#include "session.h"

#include <nic_session/rpc_object.h>
#include <nic/packet_allocator.h>

#include <packet_handler.h>

namespace Net {
	using namespace Genode;

	class Uplink_component;
	class Uplink;
}


class Net::Uplink_component : private ::Net::Session_resources,
                              public ::Nic::Session_rpc_object
{
	private:

		Vlan           &_vlan;
		Packet_handler &_packet_handler;

		Mac_address const _mac;
		Link_state _link_state { LINK_DOWN };
		Genode::Signal_context_capability _link_sigh { };

	public:

		Uplink_component(Vlan &vlan,
		                 Packet_handler &packet_handler,
		                 Genode::Env &env, Mac_address mac,
		                 Genode::Ram_quota ram_quota,
		                 Genode::Cap_quota cap_quota,
		                 Nic::Session::Tx_size tx_size,
		                 Nic::Session::Rx_size rx_size)
		: Session_resources(env.pd(), env.rm(),
		                    ram_quota, cap_quota,
		                    tx_size, rx_size),
		  Session_rpc_object(env.rm(), _tx_ds.cap(), _rx_ds.cap(),
		                     &_packet_alloc, env.ep().rpc_ep()),
		  _vlan(vlan), _packet_handler(packet_handler), _mac(mac)
		{
			_tx.sigh_ready_to_ack(_packet_handler._sink_ack);
			_tx.sigh_packet_avail(_packet_handler._sink_submit);
			_rx.sigh_ack_avail(_packet_handler._source_ack);
			_rx.sigh_ready_to_submit(_packet_handler._source_submit);
		}

		virtual ~Uplink_component() { }

		Mac_address mac_address() override {
			return _mac; }

		void link_state(Link_state) override;

		Link_state session_link_state() override {
			return _link_state; }

		void link_state_sigh(Genode::Signal_context_capability sigh) override {
			_link_sigh = sigh; }

		Packet_stream_sink<::Nic::Session::Policy> * sink() {
			return _tx.sink(); }

		Packet_stream_source<::Nic::Session::Policy> * source() {
			return _rx.source(); }

};


class Net::Uplink : public Net::Packet_handler
{
	private:

		Vlan &_vlan;

		Genode::Constructible<Uplink_component> _session { };

	public:

		Uplink(Genode::Entrypoint &ep, Vlan &vlan,
		       bool const &verbose)
		: Packet_handler(ep, vlan, Session_label("uplink"), verbose),
		  _vlan(vlan)
		{ }

		Mac_address mac() { return _session->mac_address(); }

		Nic::Session::Link_state link_state()
		{
			return _session.constructed()
				? _session->session_link_state()
				: Nic::Session::LINK_DOWN;
		}

		Vlan &vlan() { return _vlan; }

		bool uplink_constructed() const {
			return (_session.constructed()); }

		Uplink_component&
		construct_session(Genode::Env &env, Mac_address mac,
		                  Genode::Ram_quota ram_quota,
		                  Genode::Cap_quota cap_quota,
		                  Nic::Session::Tx_size tx_size,
		                  Nic::Session::Rx_size rx_size)
		{
			if (_session.constructed()) throw Service_denied();

			_session.construct(_vlan, *this, env, mac,
			                   ram_quota, cap_quota,
			                   tx_size, rx_size);
			return *_session;
		}

		void destruct_session()
		{
			if (_session.constructed())
				_session.destruct();
		}

		/******************************
		 ** Packet_handler interface **
		 ******************************/

		Packet_stream_sink< ::Nic::Session::Policy> * sink() override {
			return _session->sink(); }

		Packet_stream_source< ::Nic::Session::Policy> * source() override {
			return _session->source(); }

		bool handle_arp(Ethernet_frame &eth,
		                Size_guard     &size_guard) override;

		bool handle_ip(Ethernet_frame &eth,
		               Size_guard     &size_guard) override;

		void finalize_packet(Ethernet_frame *, Genode::size_t) override { }

};

#endif /* _SRC__SERVER__NIC_BRIDGE__UPLINK_H_ */
