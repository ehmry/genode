/*
 * \brief  Audio_in session interface
 * \author Josef Soentgen
 * \date   2015-05-08
 *
 * An Audio_in session corresponds to one input channel, which can be used to
 * receive audio frames. Each session consists of an 'Audio_in::Stream' object
 * that resides in shared memory between the client and the server. The
 * 'Audio_in::Stream' in turn consists of 'Audio_in::Packet's that contain
 * the actual frames. Each packet within a stream is freely accessible. When
 * recording the source will allocate a new packet and override already
 * recorded ones if the queue is already full.
 */

/*
 * Copyright (C) 2015-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__AUDIO_IN_SESSION__AUDIO_IN_SESSION_H_
#define _INCLUDE__AUDIO_IN_SESSION__AUDIO_IN_SESSION_H_

#include <audio/stream.h>
#include <base/allocator.h>
#include <dataspace/capability.h>
#include <base/rpc.h>
#include <session/session.h>


namespace Audio_in {
	using namespace Audio;
	struct Session;
}

/**
 * Audio_in session base
 */
struct Audio_in::Session : Genode::Session
{
		/**
		 * \noapi
		 */
		static const char *service_name() { return "Audio_in"; }

		enum { CAP_QUOTA = 4 };

		/**
		 * Start recording (alloc and submit packets after calling 'start')
		 */
		virtual void start() = 0;

		/**
		 * Stop recording
		 */
		virtual void stop() = 0;


		/*************
		 ** Signals **
		 *************/

		/**
		 * The 'underrun' signal is sent from the client to the server when
		 * the number of packets in a queue falls below a threshold.
		 * The recommended threshold is as Audio::UNDERRUN_THRESHOLD.
		 */
		virtual Genode::Signal_context_capability underrun_sigh() = 0;

		/**
		 * The 'reset' signal is sent from the server to the client if
		 * the sesion must undergo a reset due to a logical or physical
		 * reconfiguration. To handle this signal a client must
		 * reconfigure all other signal and call 'start' to continue.
		 */
		virtual void reset_sigh(Genode::Signal_context_capability sigh) = 0;

		GENODE_RPC(Rpc_start, void, start);
		GENODE_RPC(Rpc_stop, void, stop);
		GENODE_RPC(Rpc_dataspace, Genode::Dataspace_capability, dataspace);
		GENODE_RPC(Rpc_underrun_sigh, Genode::Signal_context_capability, underrun_sigh);
		GENODE_RPC(Rpc_reset_sigh, void, reset_sigh, Genode::Signal_context_capability);

		GENODE_RPC_INTERFACE(Rpc_start, Rpc_stop, Rpc_dataspace,
		                     Rpc_underrun_sigh, Rpc_reset_sigh);
};

#endif /* _INCLUDE__AUDIO_IN_SESSION__AUDIO_IN_SESSION_H_ */
