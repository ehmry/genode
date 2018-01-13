/*
 * \brief  Server that measures duration of LOG messages events
 * \author Emery Hemingway
 * \date   2018-01-12
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <libc/component.h>
#include <timer_session/connection.h>
#include <os/session_policy.h>
#include <log_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <root/component.h>
#include <base/session_label.h>
#include <base/heap.h>
#include <base/log.h>

/* PCRE includes */
#include <pcre.h>

namespace Log_duration {
	using namespace Genode;
	class Session_component;
	class Root_component;
	struct Main;
}


class Log_duration::Session_component : public Rpc_object<Log_session>
{
	private:

		Timer::Connection &_timer;

		pcre *_re;

		unsigned long const _start_ms = _timer.elapsed_ms();

	public:

		Session_component(Timer::Connection &timer, pcre *re)
		: _timer(timer), _re(re) { }


		/***************************
		 ** Log session interface **
		 ***************************/

		size_t write(Log_session::String const &msg) override
		{
			size_t n = Genode::strlen(msg.string());
			while (n > 0 && msg.string()[n-1] == '\n')
				--n;

			int rc = pcre_exec(_re, NULL, msg.string(), n, 0, 0, NULL, 0);
			if (rc > -1) {
				unsigned long now_ms = _timer.elapsed_ms();
				unsigned long dur = (now_ms - _start_ms);
				log(dur / 1000, ".", dur % 1000, "s: ",
				    Cstring(msg.string(), n));
			}

			return n;
		}
};


class Log_duration::Root_component :
	public Genode::Root_component<Log_duration::Session_component>
{
	private:

		Env &_env;

		Attached_rom_dataspace _config_rom { _env, "config" };

		Timer::Connection _timer { _env };

	protected:

		Log_duration::Session_component *_create_session(char const *args) override
		{
			Session_label const label = label_from_args(args);
			try {
				Session_policy const policy(label, _config_rom.xml());
				auto pattern = policy.attribute_value(
					"pattern", Genode::String<256>());
				if (pattern == "") {
					error("no pattern defined for policy: ", policy);
					throw Service_denied();
				}

				char const *errormsg = "";
				int erroroffset = 0;

				pcre *re = pcre_compile(
					pattern.string(),               /* the pattern */
					0,                     /* default options */
					&errormsg,          /* for error number */
					&erroroffset,          /* for error offset */
					NULL);                 /* use default compile context */

				if (re == NULL) {
					error("\"", pattern, "\" failed to compile, ", errormsg);
					throw Service_denied();
				}

				return new (md_alloc())
					Session_component(_timer, re);
			} catch (Session_policy::No_policy_defined) {
				error("no policy matched for '", label, "'");
				throw Service_denied();
			}
		}

	public:

		Root_component(Env &env, Allocator &alloc)
		:
			Genode::Root_component<Session_component>(env.ep(), alloc),
			_env(env)
		{ }
};


struct Log_duration::Main
{
	Genode::Env &env;

	Genode::Sliced_heap sliced_heap { env.ram(), env.rm() };

	Log_duration::Root_component log_root {
		env, sliced_heap };

	Main(Genode::Env &env) : env(env)
	{
		env.parent().announce(env.ep().manage(log_root));
	}
};


void Libc::Component::construct(Libc::Env &env) {
	static Log_duration::Main inst(env); }
