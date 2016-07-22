/*
 * \brief  NIC session interface
 * \author Norman Feske
 * \date   2009-11-13
 */

/*
 * Copyright (C) 2009-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NIC_SESSION__NIC_SESSION_H_
#define _INCLUDE__NIC_SESSION__NIC_SESSION_H_

#include <dataspace/capability.h>
#include <base/signal.h>
#include <base/rpc.h>
#include <session/session.h>
#include <packet_stream_tx/packet_stream_tx.h>
#include <packet_stream_rx/packet_stream_rx.h>
#include <rom_session/capability.h>
#include <base/output.h>

namespace Nic {

	struct Mac_address;
	struct Session;

	using Genode::Packet_stream_sink;
	using Genode::Packet_stream_source;

	typedef Genode::Packet_descriptor Packet_descriptor;
}


struct Nic::Mac_address
{
	char addr[6];

	void print(Genode::Output &out) const
	{
		using namespace Genode;

		Genode::print(out, Hex(uint8_t(addr[0]), Hex::OMIT_PREFIX, Hex::PAD)); out.out_char(':');
		Genode::print(out, Hex(uint8_t(addr[1]), Hex::OMIT_PREFIX, Hex::PAD)); out.out_char(':');
		Genode::print(out, Hex(uint8_t(addr[2]), Hex::OMIT_PREFIX, Hex::PAD)); out.out_char(':');
		Genode::print(out, Hex(uint8_t(addr[3]), Hex::OMIT_PREFIX, Hex::PAD)); out.out_char(':');
		Genode::print(out, Hex(uint8_t(addr[4]), Hex::OMIT_PREFIX, Hex::PAD)); out.out_char(':');
		Genode::print(out, Hex(uint8_t(addr[5]), Hex::OMIT_PREFIX, Hex::PAD));
	}
};


/*
 * NIC session interface
 *
 * A NIC session corresponds to a network adaptor, which can be used to
 * transmit and receive network packets. Payload is communicated over the
 * packet-stream interface set up between 'Session_client' and
 * 'Session_server'.
 *
 * Even though the methods 'tx', 'tx_channel', 'rx', and 'rx_channel' are
 * specific for the client side of the NIC session interface, they are part of
 * the abstract 'Session' class to enable the client-side use of the NIC
 * interface via a pointer to the abstract 'Session' class. This way, we can
 * transparently co-locate the packet-stream server with the client in same
 * program.
 */
struct Nic::Session : Genode::Session
{
	enum { QUEUE_SIZE = 1024 };

	/*
	 * Types used by the client stub code and server implementation
	 *
	 * The acknowledgement queue has always the same size as the submit
	 * queue. We access the packet content as a char pointer.
	 */
	typedef Genode::Packet_stream_policy<Genode::Packet_descriptor,
	                                     QUEUE_SIZE, QUEUE_SIZE, char> Policy;

	typedef Packet_stream_tx::Channel<Policy> Tx;
	typedef Packet_stream_rx::Channel<Policy> Rx;

	static const char *service_name() { return "Nic"; }

	virtual ~Session() { }

	/**
	 * Request packet-transmission channel
	 */
	virtual Tx *tx_channel() { return 0; }

	/**
	 * Request packet-reception channel
	 */
	virtual Rx *rx_channel() { return 0; }

	/**
	 * Request client-side packet-stream interface of tx channel
	 */
	virtual Tx::Source *tx() { return 0; }

	/**
	 * Request client-side packet-stream interface of rx channel
	 */
	virtual Rx::Sink *rx() { return 0; }

	/**
	 * Request interface state ROM sub-session
	 *
	 * The ROM should contain an XML structure with at least
	 * these elements:
	 *
	 * <nic link_state="true" mac_addr="00:01:02:03:04:05"/>
	 *
	 * For security and stability reasons clients should
	 * trust any out-of-band configuration in this ROM
	 * over in-band configuration from the session stream
	 * (DHCP, NDP). This is to avoid erronous or malicous
	 * addressing and routing.
	 *
	 * Servers are expected to implement the 'update'
	 * method of the ROM interface rather than serve
	 * successive dataspaces.
	 */
	virtual Genode::Rom_session_capability state_rom() = 0;

	/*******************
	 ** RPC interface **
	 *******************/

	GENODE_RPC(Rpc_tx_cap, Genode::Capability<Tx>, _tx_cap);
	GENODE_RPC(Rpc_rx_cap, Genode::Capability<Rx>, _rx_cap);
	GENODE_RPC(Rpc_state_rom, Genode::Rom_session_capability, state_rom);

	GENODE_RPC_INTERFACE(Rpc_tx_cap, Rpc_rx_cap, Rpc_state_rom);
};

#endif /* _INCLUDE__NIC_SESSION__NIC_SESSION_H_ */
