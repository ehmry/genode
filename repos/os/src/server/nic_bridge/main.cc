/*
 * \brief  Proxy-ARP for Nic-session
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include "component.h"
#include "session_requests.h"

/* Genode */
#include <nic/xml_node.h> /* ugly template dependency forces us
                             to include this before xml_node.h */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/env.h>
#include <base/log.h>
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>

namespace Nic_bridge {
	using namespace Net;
	struct Main;

	static
	Mac_address mac_from_args(char const *args)
	{
		Mac_address mac { };
		char string[24] { 0 };
		try {
			Arg_string::find_arg(args, "mac").string(string, sizeof(string), "");
			ascii_to(string, mac);
		} catch (...) { }
		return mac;
	}
}

struct Nic_bridge::Main : Genode::Session_request_handler
{
	enum { DEFAULT_MAC = 0x02 };

	Genode::Env                    &_env;
	Genode::Entrypoint             &_ep        { _env.ep() };
	Session_space                   _sessions  { };

	Genode::Heap                    _heap      { _env.ram(), _env.rm() };
	Genode::Attached_rom_dataspace  _config    { _env, "config" };
	bool                            _verbose   { _config.xml().attribute_value("verbose", false) };

	Mac_allocator                   _mac_alloc {
		Mac_address(_config.xml().attribute_value("mac", Mac_address(DEFAULT_MAC))) };
	Net::Vlan                       _vlan { };
	Net::Uplink                     _uplink { _ep, _vlan, _verbose };

	Session_requests_rom _session_requests { _env, *this };

	Parent::Server::Id _uplink_session_id { ~0UL };

	Main(Genode::Env &e) : _env(e)
	{
		/* process any requests that have already queued */
		_session_requests.schedule();
	}

	void handle_session_create(Session_state::Name const &name,
	                           Parent::Server::Id sid,
	                           Session_state::Args const &args) override
	{
		if (name != "Nic") throw Service_denied();

		Session_label  const label  { label_from_args(args.string()) };
		Session_policy const policy { label, _config.xml() };
		Mac_address mac = policy.attribute_value("mac", Mac_address());
		bool uplink =  policy.attribute_value("uplink", false);

		/* uplinks may choose their MAC */
		if (uplink)
			mac = mac_from_args(args.string());

		/* downlinks can not */
		if (!uplink && mac != Mac_address())
			mac = Mac_address();

		if (mac == Mac_address()) {
			try { mac = _mac_alloc.alloc(); }
			catch (Mac_allocator::Alloc_failed) {
				Genode::warning("MAC address allocation failed!");
				throw Service_denied();
			}
		} else if (_mac_alloc.mac_managed_by_allocator(mac)) {
			Genode::warning("MAC address already in use");
			throw Service_denied();
		}

		Ram_quota ram_quota = ram_quota_from_args(args.string());
		Cap_quota cap_quota = cap_quota_from_args(args.string());

		Nic::Session::Tx_size tx_size {
			Arg_string::find_arg(args.string(), "tx_buf_size").ulong_value(0) };
		Nic::Session::Rx_size rx_size {
			Arg_string::find_arg(args.string(), "rx_buf_size").ulong_value(0) };

		if (uplink) {
			/* defer session creation if an uplink is active */
			if (_uplink.uplink_constructed()) return;

			Uplink_component &session = _uplink.construct_session(
				_env, mac, ram_quota, cap_quota, tx_size, rx_size);
				_env.parent().deliver_session_cap(
				sid, _env.ep().manage(session));
			_uplink_session_id = sid;
		} else {
			Session_component *session = new (_heap)
				Session_component(_sessions,
				                  Session_space::Id{sid.value},
				                  _env.ram(), _env.rm(), _env.ep(),
				                  ram_quota, cap_quota,
				                  tx_size, rx_size,
				                  mac, _uplink, _verbose, label,
				                  policy.attribute_value("ip_addr", Session_component::Ip_addr()));
			_env.parent().deliver_session_cap(
				sid, _env.ep().manage(*session));
		}
	}

	void handle_session_close(Parent::Server::Id sid) override
	{
		if (_uplink_session_id == sid) {
			_uplink.destruct_session();
			_uplink_session_id = Parent::Server::Id{~0UL};
		}

		auto close_downlink = [&] (Session_component &session)
		{
			_env.ep().dissolve(session);
			destroy(_heap, &session);
			_env.parent().session_response(sid, Parent::SESSION_CLOSED);
		};

		Session_space::Id id { sid.value };
		try { _sessions.apply<Session_component&>(id, close_downlink); }
		catch (Session_space::Unknown_id) { }
	}

};


void Component::construct(Genode::Env &env)
{
	static Nic_bridge::Main nic_bridge(env);
	env.parent().announce("Nic");
}
