/*
 * \brief  Terminal session that directs output to a LOG session
 * \author Norman Feske
 * \author Emery Hemingway
 * \date   2015-05-16
 */

/*
 * Copyright (C) 2013 - 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _OS__LOG_TERMINAL_SESSION__H_
#define _OS__LOG_TERMINAL_SESSION__H_

/* Genode includes. */
#include <terminal_session/terminal_session.h>
#include <log_session/log_session.h>
#include <base/rpc_server.h>
#include <os/attached_ram_dataspace.h>


namespace Terminal {

	class Buffered_output;
	class Session_component;

	using namespace Genode;

};

/**
 * Utility for the buffered output of small successive write operations
 */
class Terminal::Buffered_output
{
	private:

		enum { SIZE = Log_session::String::MAX_SIZE };

		typedef Genode::size_t  size_t;

		Genode::Log_session &_log;

		char _buf[SIZE];

		/*
		 * Index of next character within '_buf' to write
		 * and length of string to send to log.
		 */
		unsigned _index = 0;

		void _flush()
		{
			/* append null termination */
			_buf[_index] = 0;

			/* flush buffered characters to LOG */
			_log.write(Genode::Log_session::String(_buf, _index+1));

			/* reset */
			_index = 0;
		}

		size_t _remaining_capacity() const { return (SIZE-1) - _index; }

	public:

		Buffered_output(Genode::Log_session &log)
		: _log(log) { }

		~Buffered_output()
		{
			if (_index) {
				if (_remaining_capacity() && _buf[_index] != '\n')
					_buf[++_index] = '\n';
				_flush();
			}
		}

		size_t write(char const *src, size_t num_bytes)
		{
			size_t const consume_bytes = Genode::min(num_bytes,
			                                         _remaining_capacity());

			for (unsigned i = 0; i < consume_bytes; i++) {
				char const c = src[i];
				_buf[_index++] = c;
				if (c == '\n')
					_flush();
			}

			if (_remaining_capacity() == 0)
				_flush();

			return consume_bytes;
		}
};

class Terminal::Session_component : public Rpc_object<Session, Session_component>
{
	private:

		/**
		 * Buffer shared with the terminal client
		 */
		Attached_ram_dataspace _io_buffer;

		Buffered_output _output;

	public:

		Session_component(Log_session &log, size_t io_buffer_size)
		:
			_io_buffer(env()->ram_session(), io_buffer_size),
			_output(log)
		{ }


		/********************************
		 ** Terminal session interface **
		 ********************************/

		Size size() { return Size(0, 0); }

		bool avail() { return false; }

		size_t _read(size_t dst_len) { return 0; }

		void _write(Genode::size_t num_bytes)
		{
			/* sanitize argument */
			num_bytes = Genode::min(num_bytes, _io_buffer.size());

			char const *src = _io_buffer.local_addr<char>();

			for (size_t written_bytes = 0; written_bytes < num_bytes; )
				written_bytes += _output.write(src + written_bytes,
				                               num_bytes - written_bytes);
		}

		Dataspace_capability _dataspace() { return _io_buffer.cap(); }

		void read_avail_sigh(Signal_context_capability) { }

		void connected_sigh(Signal_context_capability sigh)
		{
			/*
			 * Immediately reflect connection-established signal to the
			 * client because the session is ready to use immediately after
			 * creation.
			 */
			Signal_transmitter(sigh).submit();
		}

		size_t read(void *buf, size_t) { return 0; }
		size_t write(void const *buf, size_t) { return 0; }
};

#endif