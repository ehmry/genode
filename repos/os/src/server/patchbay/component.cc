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
};

namespace Source {
	using namespace Patchbay;
	using namespace Audio_in;
	class Session_component;
}

namespace Sink {
	using namespace Patchbay;
	using namespace Audio_out;
	class Session_component;
}


struct Patchbay::Session_base
{
	public:

		Session_space::Element _session_elem;

		Session_base(Session_space &space, Session_space::Id id)
		: _session_elem(*this, space, id) { }
};


class Source::Session_component final : public  Audio_in::Session_rpc_object,
                                        private Patchbay::Session_base
{
	public:

		Session_component(Genode::Env &env,
		                  Genode::Signal_context_capability data_cap,
		                  Session_space &space, Session_space::Id id)
		: Audio_in::Session_rpc_object(env, data_cap),
		  Session_base(space, id)
		{ }
};


class Sink::Session_component final : public  Audio_out::Session_rpc_object,
                                      private Patchbay::Session_base
{
	public:

		Session_component(Genode::Env &env,
		                  Genode::Signal_context_capability data_cap,
		                  Session_space &space, Session_space::Id id)
		: Audio_out::Session_rpc_object(env, data_cap),
		  Session_base(space, id)
		{ }
};


struct Patchbay::Main : Genode::Session_request_handler
{
	Genode::Env &env;

	Sliced_heap session_heap { env.ram(), env.rm() };

	Session_space sessions { };

	Session_requests_rom session_requests { env, *this };

	void handle_session_create(Session_state::Name const &name,
	                           Parent::Server::Id pid,
	                           Session_state::Args const &args) override
	{
		PDBG(name, " ", args);
		Session_space::Id id { pid.value };

		if (name == "Audio_out") {
			using namespace Sink;
			Session_component * session = new (session_heap)
				Session_component(env, Genode::Signal_context_capability(), sessions, id);

			Audio_out::Session_capability cap = env.ep().manage(*session);
			env.parent().deliver_session_cap(pid, cap);
		}

		else

		if (name == "Audio_in") {
			using namespace Source;
			Session_component * session = new (session_heap)
				Session_component(env, Genode::Signal_context_capability(), sessions, id);

			Audio_in::Session_capability cap = env.ep().manage(*session);
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
