/*
 * \brief  Client-side interface for packet-stream reception
 * \author Norman Feske
 * \date   2010-03-01
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PACKET_STREAM_TX__CLIENT_H_
#define _INCLUDE__PACKET_STREAM_TX__CLIENT_H_

#include <packet_stream_tx/packet_stream_tx.h>
#include <base/rpc_client.h>

namespace Packet_stream_tx { template <typename> class Client; }


template <typename CHANNEL>
class Packet_stream_tx::Client : public Genode::Rpc_client<CHANNEL>
{
	private:

		/*
		 * Type shortcuts
		 */
		typedef Genode::Rpc_client<CHANNEL>       Base;
		typedef typename Base::Rpc_dataspace      Rpc_dataspace;
		typedef typename Base::Rpc_server_sigh    Rpc_server_sigh;
		typedef typename Base::Rpc_client_sigh    Rpc_client_sigh;
		typedef typename Base::Rpc_client_io_sigh Rpc_client_io_sigh;

		/**
		 * Packet-stream source
		 */
		typename CHANNEL::Source _source;

	public:

		/**
		 * Constructor
		 *
		 * \param buffer_alloc  allocator used for managing the
		 *                      transmission buffer
		 */
		Client(Genode::Capability<CHANNEL> channel_cap,
		       Genode::Region_map &rm,
		       Genode::Range_allocator &buffer_alloc)
		:
			Genode::Rpc_client<CHANNEL>(channel_cap),
			_source(Base::template call<Rpc_dataspace>(), rm, buffer_alloc, true)
		{
			_source.register_sigh(Base::template call<Rpc_server_sigh>());
			Base::template call<Rpc_client_sigh>(_source.local_sigh());
		}

		void io_sigh(Genode::Signal_context_capability sigh) override {
			Base::template call<Rpc_client_io_sigh>(sigh); }

		typename CHANNEL::Source *source() { return &_source; }
};

#endif /* _INCLUDE__PACKET_STREAM_TX__CLIENT_H_ */
