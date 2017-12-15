/*
 * \brief  Server-side interface for packet-stream reception
 * \author Norman Feske
 * \date   2010-03-01
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PACKET_STREAM_RX__RPC_OBJECT_H_
#define _INCLUDE__PACKET_STREAM_RX__RPC_OBJECT_H_

#include <packet_stream_rx/packet_stream_rx.h>
#include <base/rpc_server.h>

namespace Packet_stream_rx { template <typename> class Rpc_object; }


template <typename CHANNEL>
class Packet_stream_rx::Rpc_object : public Genode::Rpc_object<CHANNEL, Rpc_object<CHANNEL> >
{
	private:

		Genode::Rpc_entrypoint     &_ep;
		Genode::Capability<CHANNEL> _cap;
		typename CHANNEL::Source    _source;

		Genode::Signal_context_capability _local_sigh;

	public:

		/**
		 * Constructor
		 *
		 * \param ds            dataspace used as communication buffer
		 *                      for the receive packet stream
		 * \param buffer_alloc  allocator used for managing the communication
		 *                      buffer of the receive packet stream
		 * \param ep            entry point used for serving the channel's RPC
		 *                      interface
		 */
		Rpc_object(Genode::Dataspace_capability  ds,
		           Genode::Region_map           &rm,
		           Genode::Range_allocator      &buffer_alloc,
		           Genode::Rpc_entrypoint       &ep)
		: _ep(ep), _cap(_ep.manage(this)), _source(ds, rm, buffer_alloc, false)
		{ }

		/**
		 * Destructor
		 */
		~Rpc_object() { _ep.dissolve(this); }

		/**
		 * Set signal handler for packet processing. Must be
		 * set only once and before the session capability is
		 * passed to the client.
		 */
		void local_sigh(Genode::Signal_context_capability cap)
		{
			if (_local_sigh.valid()) {
				Genode::error("server-side packet signal handler can not be set twice");
				throw ~0;
			}
			_local_sigh = cap;
		}

		typename CHANNEL::Source *source() { return &_source; }

		Genode::Capability<CHANNEL> cap() const { return _cap; }


		/*******************
		 ** RPC functions **
		 *******************/

		Genode::Dataspace_capability dataspace() { return _source.dataspace(); }

		/**
		 * Not part of the outward API, but the capability used by
		 * the client to request the server to process packets.
		 */
		Genode::Signal_context_capability server_sigh()
		{
			if (!_local_sigh.valid()) {
				Genode::error("server-side packet signal handler has not been set");
				throw ~0;
			}
			return _local_sigh;
		}

		/**
		 * Not part of the outward API, but the capability of the
		 * Signal_receiver the client uses to block the application
		 * until the server processes packets.
		 */
		void sigh(Genode::Signal_context_capability cap) {
			_source.register_sigh(cap); }

		/**
		 * Register capability of a client packet processing
		 * signal handler.
		 */
		void io_sigh(Genode::Signal_context_capability cap) override {
			_source.register_io_sigh(cap); }
};

#endif /* _INCLUDE__PACKET_STREAM_RX__RPC_OBJECT_H_ */
