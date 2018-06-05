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
#include <audio_in_session/rpc_object.h>
#include <audio_out_session/rpc_object.h>
#include <audio/buffer.h>
#include <timer_session/connection.h>
#include <base/attached_ram_dataspace.h>
#include <base/component.h>

namespace Patch {
	using namespace Genode;
	using namespace Audio;
	struct Main;
}

namespace Audio_in  { class Session_component; }
namespace Audio_out { class Session_component; }


class Audio_in::Session_component : public Audio_in::Session_rpc_object
{
	public:

		Session_component(Audio::Stream_buffer &buffer,
		                  Genode::Signal_context_capability data_cap)
		: Session_rpc_object(buffer, data_cap)
		{ }
};


class Audio_out::Session_component : public Audio_out::Session_rpc_object
{
	public:

		Session_component(Audio::Stream_buffer &buffer,
		                  Genode::Signal_context_capability data_cap)
		: Session_rpc_object(buffer, data_cap)
		{ }
};


struct Patch::Main
{
	Genode::Env &env;

	Audio::Stream_buffer buffer { env.pd(), env.rm() };

	Audio_in::Session_component audio_in_component
		{ buffer, Genode::Signal_context_capability() };

	Audio_out::Session_component audio_out_component
		{ buffer, Genode::Signal_context_capability() };

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
