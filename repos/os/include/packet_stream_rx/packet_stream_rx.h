/*
 * \brief  Interface definition for packet-stream reception channel
 * \author Norman Feske
 * \date   2010-03-01
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PACKET_STREAM_RX__PACKET_STREAM_RX_H_
#define _INCLUDE__PACKET_STREAM_RX__PACKET_STREAM_RX_H_

#include <os/packet_stream.h>
#include <base/rpc.h>

namespace Packet_stream_rx { template <typename> struct Channel; }


template <typename PACKET_STREAM_POLICY>
struct Packet_stream_rx::Channel
{
	typedef Genode::Packet_stream_source<PACKET_STREAM_POLICY> Source;
	typedef Genode::Packet_stream_sink<PACKET_STREAM_POLICY>   Sink;

	/**
	 * Request reception interface
	 *
	 * See documentation of 'Packet_stream_tx::Cannel::source'.
	 */
	virtual Sink *sink() { return 0; }

	/**
	 * Register handler for signals sent from the server as it
	 * unblocks the stream state.
	 */
	virtual void io_sigh(Genode::Signal_context_capability sigh) = 0;

	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_dataspace, Genode::Dataspace_capability, dataspace);
	GENODE_RPC(Rpc_server_sigh, Genode::Signal_context_capability, server_sigh);
	GENODE_RPC(Rpc_client_sigh, void, sigh, Genode::Signal_context_capability);
	GENODE_RPC(Rpc_client_io_sigh, void, io_sigh, Genode::Signal_context_capability);

	GENODE_RPC_INTERFACE(Rpc_dataspace, Rpc_server_sigh,
	                     Rpc_client_sigh, Rpc_client_io_sigh);
};

#endif /* _INCLUDE__PACKET_STREAM_RX__PACKET_STREAM_RX_H_ */
