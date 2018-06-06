/*
 * \brief  Audio patchbay server
 * \author Emery Hemingway
 * \date   2018-05-29
 */

#include <base/debug.h>

#include <audio_in_session/rpc_object.h>
#include <audio_in_session/capability.h>
#include <audio_out_session/rpc_object.h>
#include <audio_out_session/capability.h>
#include <audio/channels.h>
#include <os/session_policy.h>
#include <base/id_space.h>
#include <base/heap.h>
#include <base/component.h>

/* local includes */
#include "session_requests.h"

namespace Patchbay {
	using namespace Genode;

	struct Session_base;
	typedef Genode::Id_space<Session_base> Session_space;

	struct Main;

	enum { CHANNEL_COUNT = 2 };
};

namespace Source {
	using namespace Patchbay;
	using namespace Audio_out;
	class Session_component;
}

namespace Sink {
	using namespace Patchbay;
	using namespace Audio_in;
	class Session_component;
}


struct Patchbay::Session_base
{
	Session_space::Element _session_elem;

	Session_label const label;

	Audio::Channels const channels;

	Session_base(Session_space &space, Session_space::Id id,
	             Session_label const &label, Audio::Channels chans)
	: _session_elem(*this, space, id), label(label), channels(chans) { }
};


class Source::Session_component final : public  Audio_out::Session_rpc_object,
                                        public Patchbay::Session_base
{
	public:

		Session_component(Genode::Env &env,
		                  Genode::Signal_context_capability data_cap,
		                  Session_space &space, Session_space::Id id,
		                  Session_label const &label,
		                  Audio::Channels channels)
		: Audio_out::Session_rpc_object(env, data_cap),
		  Session_base(space, id, label, channels)
		{
			if (!data_cap.valid()) {
				error("invalid data cap");
				throw ~0;
			}
		}
};


class Sink::Session_component final : public  Audio_in::Session_rpc_object,
                                      public Patchbay::Session_base
{
	public:

		Session_component(Genode::Env &env,
		                  Genode::Signal_context_capability data_cap,
		                  Session_space &space, Session_space::Id id,
		                  Session_label const &label,
		                  Audio::Channels channels)

		: Audio_in::Session_rpc_object(env, data_cap),
		  Session_base(space, id, label, channels)
		{ }
};


struct Patchbay::Main : Genode::Session_request_handler
{
	Sink::Session_component *sinks[CHANNEL_COUNT] { nullptr };

	Session_space sessions { };

	Genode::Env &env;

	Sliced_heap session_heap { env.ram(), env.rm() };

	Attached_rom_dataspace config_rom { env, "config" };

	Session_requests_rom session_requests { env, *this };

	Signal_handler<Main> sync_handler { env.ep(), *this, &Main::sync };

	template<typename FN>
	void for_sinks(Audio::Channels channels, FN const &fn)
	{
		for (unsigned c = 0; c < CHANNEL_COUNT; ++c) {
			if (channels.value & (1 << c) && sinks[c]) {
				log("process channel ", c);
				fn(*(sinks[c]->stream()));
			}
		}
	}

	void sync()
	{
		bool sources_active = false;

		auto const source_fn = [&] (Source::Session_component &source) {
			if (source.stopped()) return;
			sources_active = true;

			

		};

		sessions.for_each<Source::Session_component&>(source_fn);

		auto const sink_fn = [&] (Sink::Session_component &sink) {
			if (!sink.active()) {
				log("skip stopped sink ", sink.label);
				return;
			}

			Audio_in::Stream &stream = *sink.stream();
			stream.increment_position();
		};

		if (sources_active)
			sessions.for_each<Sink::Session_component&>(sink_fn);
	}

	void handle_session_create(Session_state::Name const &name,
	                           Parent::Server::Id pid,
	                           Session_state::Args const &args) override
	{
		Session_space::Id id { pid.value };
		Session_label     label = label_from_args(args.string());
		Session_policy    policy (label, config_rom.xml());

		Audio::Channels channels{policy.attribute_value("channels", 0U)};
		if (!channels.single()) {
			error("multiple channels selected for output session '", label, "'");
			throw Service_denied();
		}

		if (name == "Audio_out") {
			using namespace Source;


			Session_component * session = new (session_heap)
				Session_component(env, Genode::Signal_context_capability(), sessions, id, label, channels);

			Audio_out::Session_capability cap = env.ep().manage(*session);
			env.parent().deliver_session_cap(pid, cap);
		}

		else

		if (name == "Audio_in") {
			using namespace Sink;

			if (sinks[channels.value]) {
				error("channel ", channels.value, " already assigned, rejecting '", label, "'");
				throw Service_denied();
			}

			sinks[channels.value] = new (session_heap)
				Session_component(env, sync_handler, sessions, id, label, channels);

			Audio_in::Session_capability cap = env.ep().manage(*sinks[channels.value]);
			env.parent().deliver_session_cap(pid, cap);
		}
	}

	void handle_session_close(Parent::Server::Id pid) override
	{
		Session_space::Id id { pid.value };

		sessions.apply<Session_base&>(id, [&] (Session_base &s) {
				destroy(session_heap, &s); });

		env.parent().session_response(pid, Genode::Parent::SESSION_CLOSED);
	}

	Main(Genode::Env &env)
	: env(env)
	{
		env.parent().announce("Audio_out");
		env.parent().announce("Audio_in");

		/* process any requests that have already queued */
		session_requests.process();
	}
};


void Component::construct(Genode::Env &env)
{
	static Patchbay::Main inst(env);
}
