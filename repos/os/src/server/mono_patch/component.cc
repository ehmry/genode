/*
 * \brief  Audio patch component
 * \author Emery Hemingway
 * \author Josef Soentgen
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

using namespace Genode;

namespace Patch {
	using namespace Audio;
	struct State;
	struct Main;
}

namespace Audio_in  { class Session_component; }
namespace Audio_out { class Session_component; }


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

		Patch::State &_common_state;

		bool _stopped = true; /* state */

	public:

		Session_component(Patch::State &state)
		:
			_common_state(state)
		{ }


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

		Patch::State &_common_state;

		bool _stopped = true; /* state */

	public:

		Session_component(Patch::State &state)
		:
			_common_state(state)
		{ }


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


struct Patch::Main
{
	Genode::Env &env;

	Patch::State state { env };

	Audio_in::Session_component audio_in_component { state };

	Audio_out::Session_component audio_out_component { state };

	Static_root<Audio_in::Session> audio_in_root
		{ env.ep().manage(audio_in_component) };

	Static_root<Audio_out::Session> audio_out_root
		{ env.ep().manage(audio_out_component) };

	Main(Genode::Env &env) : env(env)
	{
		env.parent().announce(env.ep().manage(audio_in_root));
		env.parent().announce(env.ep().manage(audio_out_root));
	}
};


void Component::construct(Genode::Env &env)
{
	static Patch::Main inst(env);
}
