/*
 * \brief  Log service that writes messages to a file system.
 * \author Emery Hemingway
 * \date   2015-05-13
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes. */

#include <log_session/log_session.h>
#include <file_system_session/file_system_session.h>
#include <base/rpc_server.h>
#include <base/lock.h>
#include <base/printf.h>

namespace Log_file {

	using namespace Genode;

	class  Session_component;

}

class Log_file::Session_component : public Rpc_object<Log_session>
{
	private:

		File_system::Session    &_fs;
		File_system::File_handle _file_handle;
		Lock                     _offset_lock;
		File_system::seek_off_t  _offset;

	public:

		/**
		 * Constructor
		 */
		Session_component(File_system::Session    &fs,
		                  File_system::File_handle fh,
		                  File_system::seek_off_t  offset)
		: _fs(fs), _file_handle(fh), _offset(offset) { }

		~Session_component()
		{
			_fs.close(_file_handle);
		}

		/*****************
		 ** Log session **
		 *****************/

		/**
		 * Copy the log message to a packet and send it to the file system.
		 *
		 * This method returns before the write is acknowledged
		 * for the benefit of buffering.
		 */
		size_t write(Log_session::String const &string)
		{
			if (!(string.is_valid_string())) {
				PERR("corrupted string");
				return 0;
			}

			File_system::Session::Tx::Source &source = *_fs.tx();

			char const *msg  = string.string();
			size_t msg_len   = Genode::strlen(msg);
			size_t write_len = msg_len;

			/*
			 * If the message did not fill the incoming buffer
			 * make space to add a newline.
			 */
			if ((msg_len < Log_session::String::MAX_SIZE) &&
			    (msg[msg_len-1] != '\n'))
				++write_len;

			/* Protect the offset. */
			_offset_lock.lock();
			File_system::Packet_descriptor
				packet(source.alloc_packet(Log_session::String::MAX_SIZE),
				       0, /* The result struct. */
			    	   _file_handle,
			    	   File_system::Packet_descriptor::WRITE,
			    	   write_len,
			    	   _offset);
			_offset += write_len;
			_offset_lock.unlock();

			char *buf = source.packet_content(packet);
			memcpy(buf, msg, msg_len);

			if (msg_len != write_len)
				buf[msg_len] = '\n';

			collect_acknowledgements(source);
			source.submit_packet(packet);
			return msg_len;
		}

};
