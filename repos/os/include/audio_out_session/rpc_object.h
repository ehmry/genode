/*
 * \brief  Server-side audio-session interface
 * \author Sebastian Sumpf
 * \author Emery Hemingway
 * \date   2012-12-10
 */

/*
 * Copyright (C) 2012-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__AUDIO_OUT_SESSION__RPC_OBJECT_H_
#define _INCLUDE__AUDIO_OUT_SESSION__RPC_OBJECT_H_

#include <base/env.h>
#include <base/rpc_server.h>
#include <audio/buffer.h>
#include <audio_out_session/audio_out_session.h>


namespace Audio_out { class Session_rpc_object; }


class Audio_out::Session_rpc_object : public Genode::Rpc_object<Audio_out::Session,
                                                                Session_rpc_object>
{
	protected:

		Audio::Stream_buffer &_buffer;

		Genode::Signal_transmitter     _progress { };
		Genode::Signal_transmitter     _alloc    { };

		Genode::Signal_context_capability _data_cap;
		Genode::Signal_context_capability _reset_cap { };

		bool _stopped;       /* state */
		bool _progress_sigh; /* progress signal on/off */
		bool _alloc_sigh;    /* alloc signal on/off */

	public:

		Session_rpc_object(Audio::Stream_buffer &buffer,
		                   Genode::Signal_context_capability data_cap)
		:
			_buffer(buffer), _data_cap(data_cap),
			_stopped(true), _progress_sigh(false), _alloc_sigh(false)
		{ }

		Stream_source &stream() { return _buffer.stream_source(); }


		/**************
		 ** Signals  **
		 **************/

		void progress_sigh(Genode::Signal_context_capability sigh) override
		{
			_progress.context(sigh);
			_progress_sigh = true;
		}

		void underrun_sigh(Genode::Signal_context_capability) override { }

		Genode::Signal_context_capability data_avail_sigh() {
			return _data_cap; }

		void alloc_sigh(Genode::Signal_context_capability sigh) override
		{
			_alloc.context(sigh);
			_alloc_sigh = true;
		}

		void reset_sigh(Genode::Signal_context_capability sigh) override {
			_reset_cap = sigh; }


		/***********************
		 ** Session interface **
		 ***********************/

		void start() override { _stopped = false; }
		void stop()  override { _stopped = true; }

		Genode::Dataspace_capability dataspace() { return _buffer.cap(); }


		/**********************************
		 ** Session interface extensions **
		 **********************************/

		/**
		 * Send 'progress' signal
		 */
		void progress_submit()
		{
			if (_progress_sigh)
				_progress.submit();
		}

		/**
		 * Send 'alloc' signal
		 */
		void alloc_submit()
		{
			if (_alloc_sigh)
				_alloc.submit();
		}

		/**
		 * Send 'reset' signal
		 */
		void send_reset()
		{
			if (_reset_cap.valid())
				Genode::Signal_transmitter(_reset_cap).submit();
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

#endif /* _INCLUDE__AUDIO_OUT_SESSION__RPC_OBJECT_H_ */
