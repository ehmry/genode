/*
 * \brief  Client-side Audio_out-session
 * \author Sebastian Sumpf
 * \date   2012-12-20
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__AUDIO_OUT_SESSION__CLIENT_H_
#define _INCLUDE__AUDIO_OUT_SESSION__CLIENT_H_

#include <base/env.h>
#include <base/rpc_client.h>
#include <base/attached_dataspace.h>
#include <audio_out_session/audio_out_session.h>

namespace Audio_out {
	struct Signal;
	struct Session_client;
}


class Audio_out::Session_client : public Genode::Rpc_client<Session>
{
	private:

		Genode::Attached_dataspace _shared_ds;

		Stream_sink &_stream = *_shared_ds.local_addr<Stream_sink>();

	public:

		/**
		 * Constructor
		 *
		 * \param rm               region map for attaching shared buffer
		 * \param session          session capability
		 * \param alloc_signal     true, install 'alloc_signal' receiver
		 * \param progress_signal  true, install 'progress_signal' receiver
		 */
		Session_client(Genode::Region_map &rm,
		               Genode::Capability<Session> session)
		:
		  Genode::Rpc_client<Session>(session),
		  _shared_ds(rm, call<Rpc_dataspace>())
		{ }

		Stream_sink &stream() { return _stream; }

		/***********************
		 ** Session interface **
		 ***********************/

		void start() override {
			call<Rpc_start>(); }

		void progress_sigh(Genode::Signal_context_capability sigh) override {
			call<Rpc_progress_sigh>(sigh); }
};

#endif /* _INCLUDE__AUDIO_OUT_SESSION__CLIENT_H_ */
