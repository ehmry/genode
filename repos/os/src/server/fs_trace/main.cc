/*
 * \brief  File-system tracing utility
 * \author Emery Hemingway
 * \date   2017-08-08
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/debug.h>

/* Genode includes */
#include <file_system_session/connection.h>
#include <file_system_session/rpc_object.h>
#include <log_session/connection.h>
#include <base/heap.h>
#include <root/component.h>
#include <base/component.h>

namespace Fs_trace {
	using namespace File_system;

	class Session_component;
	class Root;

	typedef Genode::Registered<Session_component> Registered_session;
	typedef Genode::Registry<Registered_session>  Session_registry;
};


class Fs_trace::Session_component : public File_system::Session_rpc_object
{
	private:

		Genode::Allocator_avl  _fs_packet_alloc;

		/* This class comes without automatic session PAM upgrades */
		File_system::Connection_base _fs;

		class Trace_log : public Genode::Output
		{
			private:

				enum { BUF_SIZE = Genode::Log_session::MAX_STRING_LEN };

				Genode::Log_connection _log;

				char _buf[BUF_SIZE];
				unsigned _num_chars = 0;

				void _flush()
				{
					_buf[_num_chars] = '\0';
					_log.write(Genode::Log_session::String(_buf, _num_chars+1));
					_num_chars = 0;
				}

			public:

				Trace_log(Genode::Env &env)
				: _log(env, "trace") { }

				void out_char(char c) override
				{
					_buf[_num_chars++] = c;
					if (_num_chars >= sizeof(_buf)-1)
						_flush();
				}

				template <typename... ARGS>
				void log(ARGS &&... args)
				{
					Output::out_args(*this, args...);
					_flush();
				}

		} _trace;

		void _process_packets()
		{
			File_system::Session::Tx::Sink &sink = *tx_sink();
			File_system::Session::Tx::Source &source = *_fs.tx();

			while (sink.packet_avail()) {
				Packet_descriptor packet = sink.get_packet();

				Node_handle handle = packet.handle();
				Genode::size_t length = packet.length();
				Packet_descriptor::Opcode op = packet.operation();
				seek_off_t seek = packet.position();

				switch (op) {
				case Packet_descriptor::READ:
					_trace.log("re ", handle, " ", length, " ", seek);
					break;

				case Packet_descriptor::WRITE: {
					_trace.log("wr ", handle, " ", length, " ", seek);

					void *sink_buf = sink.packet_content(packet);
					void *source_buf = source.packet_content(packet);
					Genode::memcpy(source_buf, sink_buf, length);
					break;
				}

				default:
					break;
				}

				source.submit_packet(packet);
			}
		}

		void _process_acks()
		{
			File_system::Session::Tx::Source &source = *_fs.tx();
			File_system::Session::Tx::Sink &sink = *tx_sink();

			while (source.ack_avail()) {
				Packet_descriptor packet = source.get_acked_packet();
				Genode::size_t length = packet.length();
				Packet_descriptor::Opcode op = packet.operation();

				switch (op) {
				case Packet_descriptor::READ: {
					void *sink_buf = sink.packet_content(packet);
					void *source_buf = source.packet_content(packet);
					Genode::memcpy(sink_buf, source_buf, length);
					break;
				}

				default:
					break;
				}


				sink.acknowledge_packet(packet);
			}
		}

		Genode::Signal_handler<Session_component> _process_packet_handler;
		Genode::Signal_handler<Session_component> _process_ack_handler;

	public:

		/**
		 * Constructor
		 * \param ep           thead entrypoint for session
		 * \param cache        node cache
		 * \param tx_buf_size  shared transmission buffer size
		 * \param root_path    path root of the session
		 * \param writable     whether the session can modify files
		 */

		Session_component(Genode::Env           &env,
		                  Genode::Heap          &heap,
		                  Genode::Session_label &label,
		                  char                  *root,
		                  bool                   writeable,
		                  size_t                 tx_buf_size)
		:
			Session_rpc_object(env.ram().alloc(tx_buf_size), env.rm(), env.ep().rpc_ep()),
			_fs_packet_alloc(&heap),
			_fs(env, _fs_packet_alloc, label.string(), root, writeable, tx_buf_size),
			_trace(env),
			_process_packet_handler(env.ep(), *this, &Session_component::_process_packets),
			_process_ack_handler(env.ep(), *this, &Session_component::_process_acks)
		{
			/*
			 * Register '_process_packets' dispatch function as signal
			 * handler for packet-avail and ready-to-ack signals.
			 */
			_tx.sigh_packet_avail(_process_packet_handler);
			// _tx.sigh_ready_to_ack(_process_packet_handler);
			_fs.sigh_ack_avail(_process_ack_handler);
		}


		/***************************
		 ** File_system interface **
		 ***************************/

		Dir_handle dir(File_system::Path const &path, bool create) override
		{
			_trace.log("do ", path, "\t", (int)create);
			Dir_handle handle = _fs.dir(path, create);
			_trace.log("dh ", handle, " ", path);
			return handle;
		}

		File_handle file(Dir_handle dir_handle, Name const &name,
		                 Mode mode, bool create) override
		{
			_trace.log("fo ", dir_handle, " ", name, "\t", (int)mode, "\t", (int)create);
			File_handle handle = _fs.file(dir_handle, name, mode, create);
			_trace.log("fh ", handle);
			return handle;
		}

		Symlink_handle symlink(Dir_handle dir, Name const &name, bool create) override
		{
			_trace.log("sl ", dir, " ", name, "\t", create);
			Symlink_handle handle = _fs.symlink(dir, name, create);
			_trace.log("sh ", handle);
			return handle;
		}

		Node_handle node(Path const &path) override
		{
			_trace.log("no ", path);
			Node_handle handle = _fs.node(path);
			_trace.log("nh ", handle);
			return handle;
		}

		void close(Node_handle handle) override
		{
			_trace.log("cl ", handle);
			_fs.close(handle);
		}

		Status status(Node_handle handle) override
		{
			_trace.log("st ", handle);
			return _fs.status(handle);
		}

		void unlink(Dir_handle dir, Name const &name) override
		{
			_trace.log("ul ", dir, " ", name);
			_fs.unlink(dir, name);
		}

		void truncate(File_handle handle, file_size_t size) override
		{
			_trace.log("tr ", handle, " ", size);
			_fs.truncate(handle, size);
		}

		void move(Dir_handle from_dir, Name const &from_name,
		          Dir_handle to_dir,   Name const &to_name) override
		{
			_trace.log("rn ", from_dir, " ", from_name,
			           "\t", to_dir, " ", to_name);
			return _fs.move(from_dir, from_name, to_dir, to_name);
		}

		/**
		 * Sync the VFS and send any pending signals on the node.
		 */
		void sync(Node_handle handle) override
		{
			_trace.log("sy ", handle);
			return _fs.sync(handle);
		}

		void control(Node_handle, Control) override { }
};


class Fs_trace::Root :
	public Genode::Root_component<Session_component>
{
	private:

		Genode::Env  &_env;
		Genode::Heap  _heap { &_env.ram(), &_env.rm() };

	protected:

		Session_component *_create_session(const char *args) override
		{
			using namespace Genode;

			Session_label label = label_from_args(args);

			size_t tx_buf_size =
				Arg_string::find_arg(args, "tx_buf_size").aligned_size();

			char root[MAX_PATH_LEN/2];
			Arg_string::find_arg(args, "root").string(root, sizeof(root), "/");

			bool writeable = Arg_string::find_arg(args, "writeable").bool_value(false);

			Session_component *session = new (md_alloc())
				Session_component(
					_env, _heap, label, root, writeable, tx_buf_size);

			return session;
		}

		void _upgrade_session(Session_component *session,
		                      char        const *args) override {
			PDBG("need to upgrade the other side"); }

	public:

		Root(Genode::Env &env, Genode::Allocator &md_alloc)
		:
			Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env)
		{
			env.parent().announce(env.ep().manage(*this));
		}
};


void Component::construct(Genode::Env &env)
{
	static Genode::Sliced_heap sliced_heap { &env.ram(), &env.rm() };

	static Fs_trace::Root root { env, sliced_heap };
}
