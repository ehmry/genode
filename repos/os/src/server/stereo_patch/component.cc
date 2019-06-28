/*
 * \brief  Audio patch component
 * \author Emery Hemingway
 * \date   2018-06-05
 *
 * Server for patching packets from an Audio_out client to an Audio_in client.
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <audio_in_session/audio_in_session.h>
#include <audio_out_session/audio_out_session.h>
#include <audio/buffer.h>
#include <base/attached_ram_dataspace.h>
#include <base/rpc_server.h>
#include <base/component.h>

/* local includes */
#include "session_requests.h"

using namespace Genode;

namespace Patch {
	using namespace Audio;
	struct State;
	struct Main;
}

namespace Audio_in  { class Session_component; }
namespace Audio_out { class Session_component; }

typedef Parent::Server::Id Id;


struct Patch::State
{
	Audio::Stream_buffer buffer;

	Genode::Signal_context_capability    start_cap { };
	Genode::Signal_context_capability    reset_cap { };
	Genode::Signal_context_capability progress_cap { };

	State(Genode::Env &env) : buffer(env.pd(), env.rm()) { }
};


class Audio_in::Session_component :
	public Genode::Rpc_object<Audio_in::Session, Session_component>
{
	private:

		Patch::State &_common_state;

	public:

		Session_component(Patch::State &state)
		: _common_state(state) { }

		/************************
		 ** Audio_in interface **
		 ************************/

		Genode::Dataspace_capability dataspace() {
			return _common_state.buffer.cap(); }

		void start_sigh(Signal_context_capability sigh) override {
			_common_state.start_cap = sigh; }

		void reset_sigh(Signal_context_capability sigh) override {
			_common_state.reset_cap = sigh; }

		Signal_context_capability progress_sigh() override {
			return _common_state.progress_cap; }
};


class Audio_out::Session_component :
	public Genode::Rpc_object<Audio_out::Session, Session_component>
{
	private:

		Patch::State &_common_state;

	public:

		Session_component(Patch::State &state)
		: _common_state(state) { }


		/************************
		 ** Audio_in interface **
		 ************************/

		Genode::Dataspace_capability dataspace() {
			return _common_state.buffer.cap(); }

		void start() override
		{
			if (_common_state.start_cap.valid())
				Signal_transmitter(_common_state.start_cap).submit();
		}

		void progress_sigh(Signal_context_capability sigh) override
		{
			_common_state.progress_cap = sigh;
			if (_common_state.reset_cap.valid()) {
				Signal_transmitter(_common_state.reset_cap).submit();
			} else {
				Genode::warning("reset signal handler is not valid");
			}
		}
};


struct Patch::Main : Genode::Session_request_handler
{
	Genode::Env &env;

	struct State_pair
	{
		State left;
		State right;

		State_pair(Genode::Env &env)
		: left(env), right(env) { }
	};

	State_pair state { env };

	template <typename SESSION>
	struct Session_pair
	{
		Constructible<SESSION> left  { };
		Constructible<SESSION> right { };

		void deliver_session(Genode::Env &env,
		                     State_pair &state,
		                     Parent::Server::Id id,
		                     Session_label const &label)
		{
			SESSION *session = nullptr;

			if (label == "left" || label == "front left") {
				if (left.constructed()) return;
				left.construct(state.left);
				session = &(*left);
			} else
			if (label == "right" || label == "front right") {
				if (right.constructed()) return;
				right.construct(state.right);
				session = &(*right);
			} else {
				Genode::log("denying session \"", label, "\"");
				throw Service_denied();
			}

			env.parent().deliver_session_cap(
				id, env.ep().manage(*session));
		}
	};

	Session_pair< Audio_in::Session_component>  in_sessions { };
	Session_pair<Audio_out::Session_component> out_sessions { };

	Genode::Session_requests_rom session_requests { env, *this };

	Main(Genode::Env &env) : env(env)
	{
		session_requests.schedule();
	}

	void handle_session_create(Session_state::Name const &name,
	                           Parent::Server::Id id,
	                           Session_state::Args const &args) override
	{
		Session_label const label_last =
			label_from_args(args.string()).last_element();

		if (name == "Audio_in") {
			in_sessions.deliver_session(env, state, id, label_last);
			session_requests.schedule();
		} else
		if (name == "Audio_out") {
			/* these sessions must be delivered second */
			if (in_sessions.left.constructed()
			 && in_sessions.right.constructed())
			{
				out_sessions.deliver_session(env, state, id, label_last);
			}
		} else
		{
			throw Service_denied();
		}
	}

	void handle_session_close(Parent::Server::Id) override
	{
		Genode::log("client session disconnected, exiting...");
		env.parent().exit(0);
	}
};


void Component::construct(Genode::Env &env)
{
	static Patch::Main inst(env);
}
