/*
 * \brief  Client-side NIC session interface
 * \author Norman Feske
 * \date   2009-11-13
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NIC_SESSION__CLIENT_H_
#define _INCLUDE__NIC_SESSION__CLIENT_H_

#include <base/rpc_client.h>
#include <nic_session/capability.h>
#include <packet_stream_tx/client.h>
#include <packet_stream_rx/client.h>
#include <rom_session/client.h>
#include <base/attached_dataspace.h>
#include <nic/xml_node.h>
#include <util/xml_node.h>

namespace Nic { class Session_client; }


class Nic::Session_client : public Genode::Rpc_client<Session>
{
	private:

		Packet_stream_tx::Client<Tx> _tx;
		Packet_stream_rx::Client<Rx> _rx;

		Genode::Rom_session_client _state_rom;
		Genode::Attached_dataspace _state_rom_ds;

		/**
		 * Blocks until the dataspace is valid
		 *
		 * This is to ensure we can get a permanent MAC address.
		 */
		Genode::Rom_dataspace_capability _first_valid_ds()
		{
			using namespace Genode;

			Rom_dataspace_capability rom_ds =
				_state_rom.dataspace();

			if (!rom_ds.valid()) {
				Signal_context  sig_ctx;
				Signal_receiver sig_rec;
				_state_rom.sigh(sig_rec.manage(&sig_ctx));
				do {
					sig_rec.wait_for_signal();
					rom_ds = _state_rom.dataspace();
				} while (!rom_ds.valid());
				sig_rec.dissolve(&sig_ctx);
			}
			return rom_ds;
		}

	public:

		/**
		 * Constructor
		 *
		 * \param tx_buffer_alloc  allocator used for managing the
		 *                         transmission buffer
		 */
		Session_client(Session_capability       session,
		               Genode::Range_allocator &tx_buffer_alloc,
		               Genode::Region_map      &rm)
		:
			Genode::Rpc_client<Session>(session),
			_tx(call<Rpc_tx_cap>(), rm, tx_buffer_alloc),
			_rx(call<Rpc_rx_cap>(), rm),
			_state_rom(call<Rpc_state_rom>()),
			_state_rom_ds(rm, _first_valid_ds())
		{ }

		/**
		 * Retrieve state ROM
		 */
		Genode::Rom_session &rom() { return _state_rom; }

		/**
		 * Retrieve state XML
		 */
		Genode::Xml_node xml() const
		{
			using Genode::Xml_node;
			try {
				return Xml_node(_state_rom_ds.local_addr<char>(),
				                _state_rom_ds.size());
			} catch (Xml_node::Invalid_syntax) { }

			return Xml_node("<nic/>");
		}

		/**
		 * Register signal handler for state updates
		 */
		void state_sigh(Genode::Signal_context_capability sig_cap)
		{
			_state_rom.sigh(sig_cap);
		}


		/***************************
		 ** NIC session interface **
		 ***************************/

		Tx *tx_channel() { return &_tx; }
		Rx *rx_channel() { return &_rx; }
		Tx::Source *tx() { return _tx.source(); }
		Rx::Sink   *rx() { return _rx.sink(); }

		Genode::Rom_session_capability state_rom() override
		{
			return call<Rpc_state_rom>();
		}

		/**
		 * \noapi
		 * \deprecated  use the embedded ROM session
		 */
		Mac_address mac_address()
		{
			Mac_address addr;
			try { xml().attribute("mac_addr").value(&addr); }
			catch (...) { }
			return addr;
		}

		/**
		 * \noapi
		 * \deprecated  use the embedded ROM session
		 */
		void link_state_sigh(Genode::Signal_context_capability sig_cap)
		{
			_state_rom.sigh(sig_cap);
		}

		/**
		 * \noapi
		 * \deprecated  use the embedded ROM session
		 */
		bool link_state()
		{
			return xml().attribute_value("link_state", false);
		}
};

#endif /* _INCLUDE__NIC_SESSION__CLIENT_H_ */
