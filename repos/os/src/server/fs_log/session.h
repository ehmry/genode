/*
 * \brief  Log session that writes messages to a file system.
 * \author Emery Hemingway
 * \date   2015-05-16
 *
 * Message writing is fire-and-forget to prevent
 * logging from becoming I/O bound.
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FS_LOG__SESSION_H_
#define _FS_LOG__SESSION_H_

/* Genode includes */
#include <log_session/log_session.h>
#include <file_system_session/file_system_session.h>
#include <base/rpc_server.h>
#include <base/snprintf.h>
#include <base/log.h>

namespace Fs_log {

	enum { MAX_LABEL_LEN = 128 };

	class Session_component;
}

class Fs_log::Session_component : public Genode::Rpc_object<Genode::Log_session>
{
	private:

		typedef Genode::String<Log_session::String::MAX_SIZE> String;

		File_system::Session           &_fs;
		File_system::File_handle const  _handle;
		String                   const  _label;

	public:

		Session_component(File_system::Session     &fs,
		                  File_system::File_handle  handle,
		                  char               const *label)
		:
			_fs(fs), _handle(handle),
			_label(Genode::strlen(label) ? String("[", label, "] ") : "")
		{ }

		~Session_component()
		{
			/* sync */

			File_system::Session::Tx::Source &source = *_fs.tx();

			File_system::Packet_descriptor packet = source.get_acked_packet();

			if (packet.operation() == File_system::Packet_descriptor::SYNC)
				_fs.close(packet.handle());

			packet = File_system::Packet_descriptor(
				packet, _handle, File_system::Packet_descriptor::SYNC, 0, 0);

			source.submit_packet(packet);
		}


		/*****************
		 ** Log session **
		 *****************/

		Genode::size_t write(Log_session::String const &msg)
		{
			using namespace Genode;

			if (!msg.is_valid_string()) {
				Genode::error("received corrupted string");
				return 0;
			}

			File_system::Session::Tx::Source &source = *_fs.tx();

			File_system::Packet_descriptor packet = source.get_acked_packet();

			if (packet.operation() == File_system::Packet_descriptor::SYNC)
				_fs.close(packet.handle());

			size_t msg_len = strlen(msg.string());
			size_t pkt_len = 0;
			source.apply_payload(packet, [&] (char *buf, size_t len) {
				if (_label == "") {
					pkt_len = min(len, msg_len);
					memcpy(buf, msg.string(), pkt_len);
				} else {
					String tmp(_label, msg.string());
					pkt_len = min(len, tmp.length()-1);
					memcpy(buf, tmp.string(), pkt_len);
					msg_len = msg_len - (_label.length()-1);
				}
			});

			packet = File_system::Packet_descriptor(
				packet, _handle, File_system::Packet_descriptor::WRITE,
				pkt_len, File_system::SEEK_TAIL);
			source.submit_packet(packet);

			return msg_len;
		}
};

#endif
