/*
 * \brief  NIC driver based on iPXE
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2011-11-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <nic/component.h>
#include <nic/root.h>
#include <base/component.h>
#include <base/log.h>

#include <dde_ipxe/nic.h>


class Ipxe_session_component  : public Nic::Session_component
{
	public:

		static Ipxe_session_component *instance;

	private:

		Nic::State_component           _state_rom;
		Genode::Rom_session_capability _state_cap;

		static void _rx_callback(unsigned    if_index,
		                         const char *packet,
		                         unsigned    packet_len)
		{
			if (instance)
				instance->_receive(packet, packet_len);
		}

		static void _link_callback()
		{
			if (instance) {
				instance->_state_rom.link_state(dde_ipxe_nic_link_state(1));
				instance->_state_rom.submit_signal();
			}
		}

		bool _send()
		{
			using namespace Genode;

			if (!_tx.sink()->ready_to_ack())
				return false;

			if (!_tx.sink()->packet_avail())
				return false;

			Packet_descriptor packet = _tx.sink()->get_packet();
			if (!packet.size()) {
				Genode::warning("Invalid tx packet");
				return true;
			}

			if (dde_ipxe_nic_tx(1, _tx.sink()->packet_content(packet), packet.size()))
				Genode::warning("Sending packet failed!");

			_tx.sink()->acknowledge_packet(packet);
			return true;
		}

		void _receive(const char *packet, unsigned packet_len)
		{
			_handle_packet_stream();

			if (!_rx.source()->ready_to_submit())
				return;

			try {
				Nic::Packet_descriptor p = _rx.source()->alloc_packet(packet_len);
				Genode::memcpy(_rx.source()->packet_content(p), packet, packet_len);
				_rx.source()->submit_packet(p);
			} catch (...) {
				Genode::warning("failed to process received packet");
			}
		}

		void _handle_packet_stream() override
		{
			while (_rx.source()->ack_avail())
				_rx.source()->release_packet(_rx.source()->get_acked_packet());

			while (_send()) ;
		}

	public:

		Ipxe_session_component(Genode::size_t const tx_buf_size,
		                       Genode::size_t const rx_buf_size,
		                       Genode::Allocator   &rx_block_md_alloc,
		                       Genode::Ram_session &ram,
		                       Genode::Region_map  &rm,
		                       Genode::Entrypoint  &ep)
		:
			Session_component(tx_buf_size, rx_buf_size, rx_block_md_alloc, ram, rm, ep),
			_state_rom(ram, rm),
			_state_cap(ep.manage(_state_rom))
		{
			instance = this;

			Genode::log("--- init callbacks");
			dde_ipxe_nic_register_callbacks(_rx_callback, _link_callback);

			Nic::Mac_address mac;
			dde_ipxe_nic_get_mac_addr(1, mac.addr);
			_state_rom.mac_addr(mac);
			_state_rom.link_state(dde_ipxe_nic_link_state(1));
			Genode::log("--- get MAC address ", mac);
		}

		~Ipxe_session_component()
		{
			instance = nullptr;
			dde_ipxe_nic_unregister_callbacks();
		}


		/***************************
		 ** Nic session interface **
		 ***************************/

		Genode::Rom_session_capability state_rom() override {
			return _state_cap; }
};


Ipxe_session_component *Ipxe_session_component::instance;


struct Main
{
	Genode::Heap heap;

	Nic::Root<Ipxe_session_component> root;

	Main(Genode::Env &env)
	: heap(env.ram(), env.rm()), root(env, heap, heap)
	{
		Genode::log("--- iPXE NIC driver started ---\n");

		Genode::log("--- init iPXE NIC");
		int cnt = dde_ipxe_nic_init(&env.ep());
		Genode::log("    number of devices: ", cnt);

		env.parent().announce(env.ep().manage(root));
	}
};


/***************
 ** Component **
 ***************/

Genode::size_t      Component::stack_size() { return 2*1024*sizeof(long); }

void Component::construct(Genode::Env &env) { static Main inst(env);      }
