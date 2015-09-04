/*
 * \brief  Local log service
 * \author Emery Hemingway
 * \date   2015-08-26
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LOG_TRAP__LOG_POLICY_H_
#define _LOG_TRAP__LOG_POLICY_H_

/* Genode includes */
#include <log_session/connection.h>
#include <base/env.h>

namespace Log_trap {

	using namespace Genode;

	class Log_policy;

}

class Log_trap::Log_policy
{
	private:

		struct Local_log_session_component : Genode::Rpc_object<Log_session>
		{

			Genode::Log_connection  backend;
			char                    pattern[256];
			size_t                  len;

			/**
			 * Constructor
			 */
			Local_log_session_component()
			{
				try {
					config()->xml_node().attribute("pattern").value(pattern, sizeof(pattern));
				} catch (Xml_node::Nonexistent_attribute) {
					PERR("missing 'pattern' attribute on config");
					throw;
				}
				len = strlen(pattern);

				for (size_t i = 0; i < len; ++i)
					if (pattern[i] == '\\') {
						strncpy(&pattern[i], &pattern[i+1], len--);
						if (pattern[i] == '\\')
							++i;
					}
			}

			/***************************
			 ** LOG session interface **
			 ***************************/

			size_t write(String const &msg) override
			{
				size_t n = backend.write(msg);
				if (n < len) return n;

				const char *str = msg.string();

				/* if the log messages matches, exit with success */
				for (size_t i = 0; i < (n - len); ++i) {
					if (strcmp(pattern, str+i, len) == 0) {
						env()->parent()->exit(0);
					}
				}

				return n;
			}

		} _local_log_session;

		Rpc_entrypoint         &_ep;
		Log_session_capability  _log_cap;

		struct Local_log_service : Genode::Service
		{
			Log_session_capability  _log_cap;

			Local_log_service(Log_session_capability log_cap)
			: Genode::Service("LOG"), _log_cap(log_cap) { }

			Genode::Session_capability session(char const * /*args*/,
			                                   Genode::Affinity const &) {
				return _log_cap; }

			void upgrade(Genode::Session_capability, const char * /*args*/) { }
			void close(Genode::Session_capability) { }

		} _local_log_service;

	public:

		Log_policy(char const *label, Rpc_entrypoint &ep)
		:
			_local_log_session(),
			_ep(ep),
			_log_cap(_ep.manage(&_local_log_session)),
			_local_log_service(_log_cap)
		{ }

		~Log_policy() { _ep.dissolve(&_local_log_session); }

		Genode::Service *resolve_session_request(const char *service_name,
		                                         const char *args)
		{
			return Genode::strcmp(service_name, "LOG") ?
				0 : &_local_log_service;
		}
		
};

#endif