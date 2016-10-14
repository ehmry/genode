/*
 * \brief  LOG file system
 * \author Norman Feske
 * \date   2014-04-11
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__LOG_FILE_SYSTEM_H_
#define _INCLUDE__VFS__LOG_FILE_SYSTEM_H_

#include <log_session/connection.h>
#include <vfs/single_file_system.h>
#include <util/volatile_object.h>

namespace Vfs { class Log_file_system; }


class Vfs::Log_file_system : public Single_file_system
{
	private:

		typedef Genode::String<64> Label;
		Label _label;

		Genode::Lazy_volatile_object<Genode::Log_connection>     _log_connection;
		Genode::Lazy_volatile_object<Genode::Log_session_client> _log_client;

		Genode::Log_session & _log_session(Genode::Env &env)
		{
			if (_label.valid()) {
				_log_connection.construct(env, _label);
				return *_log_connection;
			} else {
				_log_client.construct(
					Genode::reinterpret_cap_cast<Genode::Log_session>
					(env.parent().session_cap(Genode::Parent::Env::log())));
				return *_log_client;
			}
		}

		Genode::Log_session &_log;

	public:

		Log_file_system(Genode::Env &env,
		                Genode::Allocator&,
		                Genode::Xml_node config)
		:
			Single_file_system(NODE_TYPE_CHAR_DEVICE, name(), config, OPEN_MODE_WRONLY),
			_label(config.attribute_value("label", Label())),
			_log(_log_session(env))
		{ }

		static const char *name() { return "log"; }


		/********************************
		 ** File I/O service interface **
		 ********************************/

		void write(Vfs_handle *handle, file_size count) override
		{
			/* count does not include the trailing '\0' */
			while (count > 0) {
				char tmp[Genode::Log_session::MAX_STRING_LEN];
				file_size n = min(count, sizeof(tmp) - 1);
				file_size cb_out = handle->write_callback(
					tmp, n, n == count ? Callback::COMPLETE
				                       : Callback::PARTIAL);
				tmp[cb_out > 0 ? cb_out : 0] = 0;
				_log.write(tmp);
				count -= cb_out;
			}
		}
};

#endif /* _INCLUDE__VFS__LOG_FILE_SYSTEM_H_ */
