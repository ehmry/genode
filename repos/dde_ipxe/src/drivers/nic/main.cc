/*
 * \brief  NIC driver based on iPXE
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \author Emery Hemingway
 * \date   2011-11-17
 */

/*
 * Copyright (C) 2011-2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode */
#include <nic/packet_allocator.h>
#include <nic_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/component.h>
#include <base/log.h>

#include <dde_ipxe/support.h>
#include <dde_ipxe/nic.h>

namespace Nic_drv {
	using namespace Genode;
	using namespace Nic;

	struct Main;
}


struct Nic_drv::Main
{
	static Nic_drv::Main *instance;

	Genode::Env  &_env;
	Genode::Heap  _heap { _env.ram(), _env.rm() };

	Mac_address ipxe_init()
	{
		Mac_address mac { };

		Genode::log("--- init iPXE NIC ---");

		/* pass Env to backend */
		dde_support_init(_env, _heap);

		if (dde_ipxe_nic_init() < 1) {
			Genode::error("could not find usable NIC device");
			_env.parent().exit(~0);
		} else {
			dde_ipxe_nic_get_mac_addr(mac.addr);
		}

		return mac;
	}

	Mac_address const _mac = ipxe_init();

	Nic::Packet_allocator _nic_tx_alloc { &_heap };

	Nic::Connection _nic {
		_env, _mac, _nic_tx_alloc,
		Nic::Connection::default_tx_size(),
		Nic::Connection::default_rx_size(),
		"iPXE"
	};

	Io_signal_handler<Main> _nic_packets_handler
		{ _env.ep(), *this, &Main::_handle_nic_packets };

	Io_signal_handler<Main> _nic_link_handler
		{ _env.ep(), *this, &Main::_handle_nic_link };

	Nic::Session::Link_state _link_state { Nic::Session::LINK_DOWN };

	void _handle_nic_link()
	{
		Nic::Session::Link_state session_link =
			_nic.session_link_state();

		if (session_link == Nic::Session::LINK_UP)
		{
			if (dde_ipxe_nic_link_state()) {
				_link_state = Nic::Session::LINK_UP;
			} else {
				_link_state = Nic::Session::LINK_DOWN;
				_nic.link_state(_link_state);
			}
		} else {
			_link_state = Nic::Session::LINK_DOWN;
		}
	}

	void _handle_nic_packets()
	{
		auto &rx = *_nic.rx();
		while (rx.packet_avail() && rx.ready_to_ack()) {
			/* XXX: loop may starve iPXE driver */
			Nic::Packet_descriptor const nic_pkt = rx.get_packet();
			if (_link_state == Nic::Session::LINK_UP)
				dde_ipxe_nic_tx(rx.packet_content(nic_pkt), nic_pkt.size());
			rx.acknowledge_packet(nic_pkt);
		}
	}

	static
	void ipxe_rx_callback(const char *packet, unsigned packet_len) {
		if (instance) instance->ipxe_receive(packet, packet_len); }

	void ipxe_receive(const char *packet,
	                  unsigned    packet_len)
	{
		if (_link_state == Nic::Session::LINK_DOWN) return;

		auto &tx = *_nic.tx();

		/* flush acknowledgements */
		while (tx.ack_avail())
			tx.release_packet(tx.get_acked_packet());

		if (!tx.ready_to_submit()) return;

		Nic::Packet_descriptor nic_pkt = tx.alloc_packet(packet_len);

		memcpy(tx.packet_content(nic_pkt), packet, packet_len);
		tx.submit_packet(nic_pkt);
	}

	static
	void ipxe_link_callback() {
		if (instance) instance->ipxe_link_state_changed(); }

	void ipxe_link_state_changed()
	{
		_nic.link_state(dde_ipxe_nic_link_state()
			? Nic::Session::LINK_UP : Nic::Session::LINK_DOWN);
	}

	Main(Genode::Env &env) : _env(env)
	{
		if (_mac != _nic.mac_address()) {
			error("Nic service does not acknowledge device MAC address");
			env.parent().exit(~0);
			return;
		}

		instance = this;

		/* set Nic session signal handlers */
		_nic.rx_channel()->sigh_packet_avail(_nic_packets_handler);
		_nic.rx_channel()->sigh_ready_to_ack(_nic_packets_handler);
		_nic.link_state_sigh(_nic_link_handler);

		dde_ipxe_nic_register_callbacks(
			ipxe_rx_callback, ipxe_link_callback);

		ipxe_link_state_changed();

		log("--- iPXE NIC driver started for ", _mac, " ---");
	}

	~Main() { dde_ipxe_nic_unregister_callbacks(); }
};


Nic_drv::Main *Nic_drv::Main::instance { nullptr };


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Nic_drv::Main inst { env };
}
