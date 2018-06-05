/*
 * \brief  Audio_out session interface
 * \author Sebastian Sumpf
 * \date   2012-12-20
 *
 * An audio session corresponds to one output channel, which can be used to
 * send audio frames. Each session consists of an 'Audio_out::Stream' object
 * that resides in shared memory between the client and the server. The
 * 'Audio_out::Stream' in turn consists of 'Audio_out::Packet's that contain
 * the actual frames. Each packet within a stream is freely accessible or may
 * be allocated successively. Also there is a current position pointer for each
 * stream that is updated by the server. This way, it is possible to send
 * sporadic events that need immediate processing as well as streams that rely
 * on buffering.
 *
 * Audio_out channel identifiers (loosely related to WAV channels) are:
 *
 * * front left (or left), front right (or right), front center
 * * lfe (low frequency effects, subwoofer)
 * * rear left, rear right, rear center
 *
 * For example, consumer-oriented 6-channel (5.1) audio uses front
 * left/right/center, rear left/right and lfe.
 *
 * Note: That most components right now only support: "(front) left" and
 * "(front) right".
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__AUDIO_OUT_SESSION__AUDIO_OUT_SESSION_H_
#define _INCLUDE__AUDIO_OUT_SESSION__AUDIO_OUT_SESSION_H_

#include <base/allocator.h>
#include <base/rpc.h>
#include <base/signal.h>
#include <dataspace/capability.h>
#include <session/session.h>
#include <audio/stream.h>


namespace Audio_out {
	using namespace Audio;
	class  Session;
};


/**
 * Audio_out session base
 */
struct Audio_out::Session : Genode::Session
{
		/**
		 * \noapi
		 */
		static const char *service_name() { return "Audio_out"; }

		enum { CAP_QUOTA = 4 };

		/**
		 * Start playback (alloc and submit packets after calling 'start')
		 */
		virtual void start() = 0;

		/**
		 * Stop playback
		 */
		virtual void stop() = 0;


		/*************
		 ** Signals **
		 *************/

		/**
		 * The 'progress' signal is sent from the server to the client if a
		 * packet has been played.
		 *
		 * See: client.h, connection.h
		 */
		virtual void progress_sigh(Genode::Signal_context_capability sigh) = 0;

		/**
		 * The 'alloc' signal is sent from the server to the client when the
		 * stream queue leaves the 'full' state.
		 *
		 * See: client.h, connection.h
		 */
		virtual void alloc_sigh(Genode::Signal_context_capability sigh)    = 0;
		
		/**
		 * The 'data_avail' signal is sent from the client to the server if the
		 * stream queue leaves the 'empty' state.
		 */
		virtual Genode::Signal_context_capability data_avail_sigh()        = 0;

		GENODE_RPC(Rpc_start, void, start);
		GENODE_RPC(Rpc_stop, void, stop);
		GENODE_RPC(Rpc_dataspace, Genode::Dataspace_capability, dataspace);
		GENODE_RPC(Rpc_progress_sigh, void, progress_sigh, Genode::Signal_context_capability);
		GENODE_RPC(Rpc_alloc_sigh, void, alloc_sigh, Genode::Signal_context_capability);
		GENODE_RPC(Rpc_data_avail_sigh, Genode::Signal_context_capability, data_avail_sigh);

		GENODE_RPC_INTERFACE(Rpc_start, Rpc_stop, Rpc_dataspace, Rpc_progress_sigh,
		                     Rpc_alloc_sigh, Rpc_data_avail_sigh);
};

#endif /* _INCLUDE__AUDIO_OUT_SESSION__AUDIO_OUT_SESSION_H_ */
