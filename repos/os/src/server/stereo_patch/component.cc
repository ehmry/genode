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
#include <os/static_root.h>
#include <audio_in_session/audio_in_session.h>
#include <audio_out_session/audio_out_session.h>
#include <audio/buffer.h>
#include <timer_session/connection.h>
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

	Genode::Signal_context_capability      overrun_cap { };
	Genode::Signal_context_capability     underrun_cap { };
	Genode::Signal_context_capability source_reset_cap { };
	Genode::Signal_context_capability   sink_reset_cap { };

	State(Genode::Env &env) : buffer(env.pd(), env.rm()) { }
};


class Audio_in::Session_component :
	public Genode::Rpc_object<Audio_in::Session, Session_component>
{
	private:

		Patch::State       &_common_state;
		Genode::Entrypoint &_ep;

		bool _stopped = true; /* state */

	public:

		Parent::Server::Id const id;

		Session_component(Patch::State &state, Entrypoint &ep, Id id)
		: _common_state(state), _ep(ep), id(id)
		{
		}

		~Session_component()
		{
			_ep.dissolve(*this);
		}


		/**************
		 ** Signals  **
		 **************/

		Genode::Signal_context_capability underrun_sigh() override { PDBG();
			return _common_state.underrun_cap; }

		void reset_sigh(Genode::Signal_context_capability sigh) override { PDBG();
			_common_state.sink_reset_cap = sigh; }

		/***********************
		 ** Session interface **
		 ***********************/

		void start() override { PDBG(); _stopped = false; }
		void stop()  override { PDBG(); _stopped = true; }

		Genode::Dataspace_capability dataspace() { PDBG();
			return _common_state.buffer.cap(); }
};


class Audio_out::Session_component :
	public Genode::Rpc_object<Audio_out::Session, Session_component>
{
	private:

		Patch::State       &_common_state;
		Genode::Entrypoint &_ep;

		bool _stopped = true; /* state */

	public:

		Id const id;

		Session_component(Patch::State &state, Entrypoint &ep, Id id)
		: _common_state(state), _ep(ep), id(id) { }

		~Session_component()
		{
			_ep.dissolve(*this);
		}

		/**************
		 ** Signals  **
		 **************/

		void underrun_sigh(Signal_context_capability sigh) override
		{ PDBG();
			_common_state.underrun_cap = sigh;
			if (_common_state.sink_reset_cap.valid())
				Signal_transmitter(_common_state.sink_reset_cap).submit();
		}

		void reset_sigh(Signal_context_capability sigh) override { PDBG();
			_common_state.source_reset_cap = sigh; }

		/***********************
		 ** Session interface **
		 ***********************/

		void start() override { PDBG(); _stopped = false; }
		void stop()  override { PDBG(); _stopped = true; }

		Genode::Dataspace_capability dataspace() { PDBG();
			return _common_state.buffer.cap(); }
};


struct Patch::Main : Genode::Session_request_handler
{
	Genode::Env &env;

	Genode::Session_requests_rom requests_rom { env, *this };

	Patch::State state_left { env };
	Patch::State state_right { env };

	Constructible<Audio_out::Session_component> source_left { };
	Constructible<Audio_out::Session_component> source_right { };

	Constructible<Audio_in::Session_component> sink_left { };
	Constructible<Audio_in::Session_component> sink_right { };

	Main(Genode::Env &env) : env(env)
	{
		requests_rom.process();
	}

	void handle_session_create(Session_state::Name const &name,
	                           Parent::Server::Id id,
	                           Session_state::Args const &args) override
	{
		Session_capability cap {};
		Session_label label_last =
			label_from_args(args.string()).last_element();
		if (name == "Audio_in") {
			if (label_last == "left") {
				sink_left.construct(state_left, env.ep(), id);
				cap = env.ep().manage(*sink_left);
			}
			else
			if (label_last == "right") {
				sink_right.construct(state_right, env.ep(), id);
				cap = env.ep().manage(*sink_right);
			}
		} else
		if (name == "Audio_out") {
			if (label_last == "left") {
				source_left.construct(state_left, env.ep(), id);
				cap = env.ep().manage(*source_left);
			}
			else
			if (label_last == "right") {
				source_right.construct(state_right, env.ep(), id);
				cap = env.ep().manage(*source_right);
			}
		}

		if (!cap.valid())
			throw Service_denied();
		env.parent().deliver_session_cap(id, cap);
	}

	void handle_session_upgrade(Parent::Server::Id,
	                            Session_state::Args const &) override
	{ }

	void handle_session_close(Parent::Server::Id id) override
	{
		if (source_left.constructed() && source_left->id == id)
			source_left.destruct();
		else
		if (source_right.constructed() && source_right->id == id)
			source_right.destruct();
		else
		if (sink_left.constructed() && sink_left->id == id)
			sink_left.destruct();
		else
		if (sink_right.constructed() && sink_right->id == id)
			sink_right.destruct();

		env.parent().session_response(id, Parent::SESSION_CLOSED);
	}

};


void Component::construct(Genode::Env &env)
{
	static Patch::Main inst(env);
}
