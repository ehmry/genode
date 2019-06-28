/*
 * \brief  Client-side Audio_in-session
 * \author Josef Soentgen
 * \date   2015-05-08
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__AUDIO_IN_SESSION__CLIENT_H_
#define _INCLUDE__AUDIO_IN_SESSION__CLIENT_H_

/* Genode includes */
#include <base/env.h>
#include <base/rpc_client.h>
#include <base/attached_dataspace.h>
#include <audio_in_session/audio_in_session.h>


namespace Audio_in {
	struct Signal;
	class Session_client;
}


class Audio_in::Session_client : public Genode::Rpc_client<Session>
{
	private:

		Genode::Attached_dataspace _shared_ds;

		Stream_source &_stream = *_shared_ds.local_addr<Stream_source>();

	public:

		/**
		 * Constructor
		 *
		 * \param session          session capability
		 * \param progress_signal  true, install 'progress_signal' receiver
		 */
		Session_client(Genode::Region_map &rm,
		               Genode::Capability<Session> session)
		:
		  Genode::Rpc_client<Session>(session),
		  _shared_ds(rm, call<Rpc_dataspace>())
		{ }

		Stream_source &stream() const { return _stream; }


		/***********************
		 ** Session interface **
		 ***********************/

		void start_sigh(Genode::Signal_context_capability sigh) override {
			call<Rpc_start_sigh>(sigh); }

		void reset_sigh(Genode::Signal_context_capability sigh) override {
			call<Rpc_reset_sigh>(sigh); }

		Genode::Signal_context_capability progress_sigh() override {
			return call<Rpc_progress_sigh>(); }

};

#endif /* _INCLUDE__AUDIO_IN_SESSION__CLIENT_H_ */
