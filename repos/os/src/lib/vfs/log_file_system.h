/*
 * \brief  LOG file system
 * \author Norman Feske
 * \date   2014-04-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__LOG_FILE_SYSTEM_H_
#define _INCLUDE__VFS__LOG_FILE_SYSTEM_H_

#include <log_session/connection.h>
#include <vfs/single_file_system.h>
#include <util/reconstructible.h>

namespace Vfs { class Log_file_system; }


class Vfs::Log_file_system : public Single_file_system
{
	private:

		char _line_buf[Genode::Log_session::MAX_STRING_LEN];
		int  _line_pos = 0;

		typedef Genode::String<64> Label;
		Label _label;

		Genode::Constructible<Genode::Log_connection>     _log_connection;
		Genode::Constructible<Genode::Log_session_client> _log_client;

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
		                Genode::Xml_node config,
		                Io_response_handler &)
		:
			Single_file_system(NODE_TYPE_CHAR_DEVICE, name(), config),
			_label(config.attribute_value("label", Label())),
			_log(_log_session(env))
		{ }

		static const char *name()   { return "log"; }
		char const *type() override { return "log"; }

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *, char const *src, file_size count,
		                   file_size &out_count) override
		{
			out_count = count;

			/* count does not include the trailing '\0' */
			while (count > 0) {
				int curr_count = min(count, ((sizeof(_line_buf) - 1) - _line_pos));

				for (int i = 0; i < curr_count; ++i) {
					if (src[i] == '\n') {
						curr_count = i+1;
						break;
					}
				}

				memcpy(_line_buf+_line_pos, src, curr_count);
				_line_pos += curr_count;

				if ((_line_pos == sizeof(_line_buf)-1) || (_line_buf[_line_pos-1] == '\n')) {
					if (_line_pos == sizeof(_line_buf)-1) {
						Genode::error("buffer is full");
					}

					int strip = 0;
					for (int i = _line_pos-1; i > 0; --i) {
						switch(_line_buf[i]) {
						case '\n':
						case '\t':
						case ' ':
							++strip;
							--_line_pos;
							break;
						default: goto strip;
						}
					}

				strip:

					_line_buf[_line_pos > 1 ? _line_pos : 0] = '\0';

					_log.write(_line_buf);
					_line_pos = 0;
				}

				count -= curr_count;
				src   += curr_count;
			}

			return WRITE_OK;
		}

		Read_result read(Vfs_handle *, char *, file_size,
		                file_size &out_count) override
		{
			out_count = 0;
			return READ_OK;
		}

		bool read_ready(Vfs_handle *) override { return false; }
};

#endif /* _INCLUDE__VFS__LOG_FILE_SYSTEM_H_ */
