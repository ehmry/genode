/*
 * \brief  Ambient entropy collector and Rng service
 * \author Emery Hemingway
 * \date   2018-11-06
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "session_requests.h"

#include <rng_session/rng_session.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>

/* jitterentropy includes */
#include <jitterentropy.h>


namespace Ambient_rng {
	using namespace Genode;

	class  Session_component;
	struct Main;

	typedef Genode::Id_space<Session_component> Session_space;

	struct Collection_failure : Genode::Exception { };
}


class Ambient_rng::Session_component final : public Genode::Rpc_object<Rng::Session>
{
	private:

		Session_component(Session_component const &);
		Session_component &operator = (Session_component const &);

		Session_space::Element  _sessions_elem;

		struct rand_data *_ec = nullptr;

	public:

		Session_component(Session_space &space, Session_space::Id id)
		: _sessions_elem(*this, space, id)
		{
			_ec = jent_entropy_collector_alloc(0, 0);
			if (!_ec) {
				Genode::error("failed to allocate jitter entropy collector");
				throw Service_denied();
			}
		}

		~Session_component()
		{
			jent_entropy_collector_free(_ec);
		}

		Genode::uint64_t gather() override
		{
			Genode::uint64_t data = 0;
			for (;;) {
				ssize_t n = jent_read_entropy(
					_ec, (char*)&data, sizeof(data));
				if (n == sizeof(data))
					break;
				else if (n < 0)
					throw Collection_failure();
			}

			return data;
		}
};


struct Ambient_rng::Main : Session_request_handler
{
	Genode::Env &_env;

	Session_space _sessions { };

	Heap        _entropy_heap { _env.pd(), _env.rm() };
	Sliced_heap _session_heap { _env.pd(), _env.rm() };

	void handle_session_create(Session_state::Name const &,
	                           Parent::Server::Id pid,
	                           Session_state::Args const &args) override
	{
		size_t ram_quota =
			Arg_string::find_arg(args.string(), "ram_quota").ulong_value(0);
		size_t session_size =
			max((size_t)4096, sizeof(Session_component));

		if (ram_quota < session_size)
			throw Insufficient_ram_quota();

		Session_space::Id id { pid.value };

		Session_component *session = new (_session_heap)
			Session_component(_sessions, id);

		_env.parent().deliver_session_cap(pid, _env.ep().manage(*session));
	}

	void handle_session_upgrade(Parent::Server::Id,
	                            Session_state::Args const &) override { }

	void handle_session_close(Parent::Server::Id pid) override
	{
		Session_space::Id id { pid.value };
		_sessions.apply<Session_component&>(
			id, [&] (Session_component &session)
		{
			_env.ep().dissolve(session);
			destroy(_session_heap, &session);
			_env.parent().session_response(pid, Parent::SESSION_CLOSED);
		});
	}

	Session_requests_rom _session_requests { _env, *this };

	Main(Genode::Env &env) : _env(env)
	{
		jitterentropy_init(_entropy_heap);
		int err = jent_entropy_init();
		if (err) {
			Genode::error("jitterentropy library could not be initialized!");
			env.parent().exit(err);
			return;
		}

		/* process any requests that have already queued */
		_session_requests.schedule();
	}
};


void Component::construct(Genode::Env &env)
{
	static Ambient_rng::Main inst(env);
}
