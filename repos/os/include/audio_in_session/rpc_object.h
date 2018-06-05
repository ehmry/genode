/*
 * \brief  Server-side Audio_in session interface
 * \author Josef Soentgen
 * \author Emery Hemingway
 * \date   2015-05-08
 */

/*
 * Copyright (C) 2015-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__AUDIO_IN_SESSION__RPC_OBJECT_H_
#define _INCLUDE__AUDIO_IN_SESSION__RPC_OBJECT_H_

/* Genode includes */
#include <base/env.h>
#include <base/rpc_server.h>
#include <audio/buffer.h>
#include <audio_in_session/audio_in_session.h>


namespace Audio_in { class Session_rpc_object; }


class Audio_in::Session_rpc_object : public Genode::Rpc_object<Audio_in::Session,
                                                               Session_rpc_object>
{
	protected:

		Audio::Stream_buffer &_buffer;

		Genode::Signal_context_capability _data_cap;
		Genode::Signal_context_capability _progress_cap { };
		Genode::Signal_context_capability _overrun_cap  { };

		bool _stopped; /* state */

	public:

		Session_rpc_object(Audio::Stream_buffer &buffer,
		                   Genode::Signal_context_capability data_cap)
		:
			_buffer(buffer), _data_cap(data_cap), _stopped(true)
		{ }

		Stream_sink &stream() { return _buffer.stream_sink(); }


		/**************
		 ** Signals  **
		 **************/

		void progress_sigh(Genode::Signal_context_capability sigh) override {
			_progress_cap = sigh; }

		void overrun_sigh(Genode::Signal_context_capability sigh) override {
			_overrun_cap = sigh; }

		Genode::Signal_context_capability data_avail_sigh() override {
			return _data_cap; }


		/***********************
		 ** Session interface **
		 ***********************/

		void start() { _stopped = false; }
		void stop()  { _stopped = true; }

		Genode::Dataspace_capability dataspace() {
			return _buffer.cap(); }


		/**********************************
		 ** Session interface extensions **
		 **********************************/

		/**
		 * Send 'progress' signal
		 */
		void progress_submit()
		{
			if (_progress_cap.valid())
				Genode::Signal_transmitter(_progress_cap).submit();
		}

		/**
		 * Send 'overrun' signal
		 */
		void overrun_submit()
		{
			if (_overrun_cap.valid())
				Genode::Signal_transmitter(_overrun_cap).submit();
		}

		/**
		 * Return true if client state is stopped
		 */
		bool stopped() const { return _stopped; }

		/**
		 * Return true if client session is active
		 */
		bool active()  const { return !_stopped; }
};

#endif /* _INCLUDE__AUDIO_IN_SESSION__RPC_OBJECT_H_ */
