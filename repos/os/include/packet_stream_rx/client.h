/*
 * \brief  Client-side interface for packet-stream transmission
 * \author Norman Feske
 * \date   2010-03-01
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PACKET_STREAM_RX__CLIENT_H_
#define _INCLUDE__PACKET_STREAM_RX__CLIENT_H_

#include <packet_stream_rx/packet_stream_rx.h>
#include <base/rpc_client.h>

namespace Packet_stream_rx { template <typename> class Client; }


template <typename CHANNEL>
class Packet_stream_rx::Client : public Genode::Rpc_client<CHANNEL>
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
		 * Packet-stream sink
		 */
		typename CHANNEL::Sink _sink;

	public:

		/**
		 * Constructor
		 */
		Client(Genode::Capability<CHANNEL> channel_cap,
		       Genode::Region_map &rm)
		:
			Genode::Rpc_client<CHANNEL>(channel_cap),
			_sink(Base::template call<Rpc_dataspace>(), rm, true)
		{
			_sink.register_sigh(Base::template call<Rpc_server_sigh>());
			Base::template call<Rpc_client_sigh>(_sink.local_sigh());
		}

		void io_sigh(Genode::Signal_context_capability sigh) override {
			Base::template call<Rpc_client_io_sigh>(sigh); }

		typename CHANNEL::Sink *sink() { return &_sink; }
};

#endif /* _INCLUDE__PACKET_STREAM_RX__CLIENT_H_ */
