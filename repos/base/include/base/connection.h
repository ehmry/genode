/*
 * \brief  Connection to a service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__CONNECTION_H_
#define _INCLUDE__BASE__CONNECTION_H_

#include <base/env.h>
#include <base/capability.h>
#include <base/log.h>

namespace Genode {

	class Connection_base;
	template <typename> class Connection;
}


class Genode::Connection_base : Noncopyable, Interface
{
	protected:

		Env &_env;

		Parent::Client _parent_client { };

		Id_space<Parent::Client>::Element const _id_space_element;

		void _block_for_session_response();

	public:

		Connection_base(Env &env)
		:
			_env(env),
			_id_space_element(_parent_client, _env.id_space())
		{ }

		void upgrade(Session::Resources resources)
		{
			String<80> const args("ram_quota=", resources.ram_quota, ", "
			                      "cap_quota=", resources.cap_quota);
			_env.upgrade(_id_space_element.id(), args.string());
		}

		void upgrade_ram(size_t bytes)
		{
			upgrade(Session::Resources { Ram_quota{bytes}, Cap_quota{0} });
		}

		void upgrade_caps(size_t caps)
		{
			upgrade(Session::Resources { Ram_quota{0}, Cap_quota{caps} });
		}
};


/**
 * Representation of an open connection to a service
 */
template <typename SESSION_TYPE>
class Genode::Connection : public Connection_base
{
	public:

		struct Args {
			Parent::Session_args session_args;
			Affinity affinity;
		};

	private:

		Args const _args;

		static
		Args _session(Affinity const &affinity,
		              const char *format_args, va_list list)
		{
			enum { FORMAT_STRING_SIZE = Parent::Session_args::MAX_SIZE };
			char session_args[FORMAT_STRING_SIZE];

			String_console sc(session_args, FORMAT_STRING_SIZE);
			sc.vprintf(format_args, list);
			va_end(list);

			return Args { session_args, affinity };
		}

		Capability<SESSION_TYPE> _request_cap()
		{
			try {
				return _env.session<SESSION_TYPE>(_id_space_element.id(),
				                                  _args.session_args,
				                                  _args.affinity);
			}
			catch (...) {
				error(SESSION_TYPE::service_name(), "-session creation failed "
				      "(", _args.session_args.string(), ")");
				throw;
			}
		}

		Capability<SESSION_TYPE> _cap = _request_cap();

	public:

		typedef SESSION_TYPE Session_type;

		/**
		 * Constructor
		 */
		Connection(Env &env, Args const &args)
		:
			Connection_base(env), _args(args), _cap(_request_cap())
		{ }

		/**
		 * Destructor
		 */
		~Connection() { _env.close(_id_space_element.id()); }

		/**
		 * Return session capability
		 */
		Capability<SESSION_TYPE> cap() const { return _cap; }

		/**
		 * Construct session argument buffer
		 */
		static Args args(const char *format_args, ...)
		{
			va_list list;
			va_start(list, format_args);

			return _session(Affinity(), format_args, list);
		}

		/**
		 * Construct session argument buffer
		 */
		static Args args(Affinity const &affinity,
		                 char     const *format_args, ...)
		{
			va_list list;
			va_start(list, format_args);

			return _session(affinity, format_args, list);
		}
};

#endif /* _INCLUDE__BASE__CONNECTION_H_ */
