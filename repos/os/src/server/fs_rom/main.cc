/*
 * \brief  Service that provides files of a file system as ROM sessions
 * \author Norman Feske
 * \author Emery Hemingway
 * \date   2013-01-11
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <rom_session/rom_session.h>
#include <file_system_session/connection.h>
#include <file_system/util.h>
#include <os/path.h>
#include <base/attached_ram_dataspace.h>
#include <root/component.h>
#include <base/component.h>
#include <base/session_label.h>
#include <util/arg_string.h>
#include <base/heap.h>


/*****************
 ** ROM service **
 *****************/

namespace Fs_rom {
	using namespace Genode;

	struct Packet_handler;

	class Rom_session_component;
	class Rom_root;

	typedef List<Rom_session_component> Sessions;

	typedef File_system::Session_client::Tx::Source Tx_source;

	enum { INVALID_HANDLE = ~0U };
}


/**
 * A 'Rom_session_component' exports a single file of the file system
 */
class Fs_rom::Rom_session_component : public  Rpc_object<Rom_session>,
                                      private Sessions::Element
{
	private:

		friend class List<Rom_session_component>;
		friend class Packet_handler;

		Env &_env;

		File_system::Session &_fs;

		enum { PATH_MAX_LEN = 512 };
		typedef Genode::Path<PATH_MAX_LEN> Path;

		/**
		 * Name of requested file, interpreted at path into the file system
		 */
		Path const _file_path;

		/**
		 * Handle for file notifications
		 */
		File_system::Watch_handle _watch_handle { INVALID_HANDLE };

		/**
		 * Handle of associated file opened during read
		 */
		File_system::File_handle _file_handle { INVALID_HANDLE };

		/**
		 * Size of current version of the file
		 */
		File_system::file_size_t _file_size = 0;

		/**
		 * Read offset of the file
		 */
		File_system::seek_off_t _file_seek = 0;

		/**
		 * Dataspace exposed as ROM module to the client
		 */
		Attached_ram_dataspace _file_ds;

		/**
		 * Signal destination for ROM file changes
		 */
		Signal_context_capability _sigh { };

		/*
		 * Version number used to track the need for ROM update notifications
		 */
		struct Version { unsigned value; };
		Version _curr_version       { 0 };
		Version _handed_out_version { ~0U };

		/**
		 * Track if the session file or a directory is being watched
		 */
		bool _watching_file = false;

		/*
		 * Exception
		 */
		struct Watch_failed { };

		/**
		 * Watch the session ROM file or some parent directory
		 */
		void _open_watch_handle()
		{
			using namespace File_system;
			if (_watch_handle.value != INVALID_HANDLE) {
				_fs.close(_watch_handle);
				_watch_handle = Watch_handle{INVALID_HANDLE};
			}

			Path watch_path(_file_path);

			/* track if we can open the file or resort to a parent */
			bool at_the_file = true;

			try {
				while (true) {
					try {
						_watch_handle = _fs.watch(watch_path.base());
						_watching_file = at_the_file;
						return;
					}
					catch (File_system::Lookup_failed) { }
					catch (File_system::Permission_denied) { }
					error("watch of ", watch_path, " failed");
					if (watch_path == "/")
						throw Watch_failed();
					watch_path.strip_last_element();
					at_the_file = false;
				}
			} catch (File_system::Out_of_ram) {
				error("not enough RAM to watch '", watch_path, "'");
			} catch (File_system::Out_of_caps) {
				error("not enough caps to watch '", watch_path, "'");
			}
			throw Watch_failed();
		}

		/**
		 * Initialize '_file_ds' dataspace with file content
		 */
		void _update_dataspace()
		{
			/*
			 * On each repeated call of this function, the dataspace is
			 * replaced with a new one that contains the most current file
			 * content.
			 */

			using namespace File_system;

			Genode::Path<PATH_MAX_LEN> dir_path(_file_path);
			dir_path.strip_last_element();
			Genode::Path<PATH_MAX_LEN> file_name(_file_path);
			file_name.keep_only_last_element();

			Dir_handle parent_handle = _fs.dir(dir_path.base(), false);
			Handle_guard parent_guard(_fs, parent_handle);

			_file_handle = _fs.file(
				parent_handle, file_name.base() + 1,
				File_system::READ_ONLY, false);
			Handle_guard file_guard(_fs, _file_handle);

			/* _file_handle will be closed upon return */

			size_t const file_size = _fs.status(_file_handle).size;

			/* allocate new RAM dataspace according to file size */
			if (file_size > 0) {
				try {
					_file_seek = 0;
					_file_ds.realloc(&_env.ram(), file_size);
					_file_size = file_size;
				} catch (...) {
					error("couldn't allocate memory for file, empty result");
					return;
				}
			}

			/* omit read if file is empty */
			if (_file_size == 0)
				return;

			/* read content from file */
			Tx_source &source = *_fs.tx();
			while (_file_seek < _file_size) {
				/* if we cannot submit then process acknowledgements */
				while (!source.ready_to_submit())
					_env.ep().wait_and_dispatch_one_io_signal();

				size_t chunk_size = min(_file_size - _file_seek,
				                        source.bulk_buffer_size() / 2);
				File_system::Packet_descriptor
					packet(source.alloc_packet(chunk_size), _file_handle,
					       File_system::Packet_descriptor::READ,
					       chunk_size, _file_seek);
				source.submit_packet(packet);

				/*
				 * Process the global signal handler until we got a response
				 * for the read request (indicated by a change of the seek
				 * position).
				 */
				size_t const orig_file_seek = _file_seek;
				while (_file_seek == orig_file_seek)
					_env.ep().wait_and_dispatch_one_io_signal();
			}
		}

		void _notify_client_about_new_version()
		{
			if (_sigh.valid() && _curr_version.value != _handed_out_version.value)
				Signal_transmitter(_sigh).submit();
		}

	public:

		/**
		 * Constructor
		 *
		 * \param fs        file-system session to read the file from
		 * \param filename  requested file name
		 * \param sig_rec   signal receiver used to get notified about changes
		 *                  within the compound directory (in the case when
		 *                  the requested file could not be found at session-
		 *                  creation time)
		 */
		Rom_session_component(Env &env,
		                      File_system::Session &fs,
		                      const char *file_path)
		:
			_env(env), _fs(fs),
			_file_path(file_path),
			_file_ds(env.ram(), env.rm(), 0) /* realloc later */
		{
			try {
				_open_watch_handle();
			} catch (Watch_failed) { }
		}

		/**
		 * Destructor
		 */
		~Rom_session_component()
		{
			if (_watch_handle.value != INVALID_HANDLE)
				_fs.close(_watch_handle);
		}

		/**
		 * Return dataspace with up-to-date content of file
		 */
		Rom_dataspace_capability dataspace()
		{
			using namespace File_system;

			try { _update_dataspace(); }
			catch (Invalid_handle)    { error(_file_path, ": Invalid_handle"); }
			catch (Invalid_name)      { error(_file_path, ": invalid_name"); }
			catch (Lookup_failed)     { error(_file_path, ": lookup_failed"); }
			catch (Permission_denied) { error(_file_path, ": Permission_denied"); }
			catch (...)               { error(_file_path, ": unhandled error"); };
			Dataspace_capability ds = _file_ds.cap();
			_handed_out_version = _curr_version;
			return static_cap_cast<Rom_dataspace>(ds);
		}

		void sigh(Signal_context_capability sigh)
		{
			_sigh = sigh;
			_notify_client_about_new_version();
		}

		/**
		 * If packet corresponds to this session then process and return true.
		 *
		 * Called from the signal handler.
		 */
		bool process_packet(File_system::Packet_descriptor const packet)
		{
			switch (packet.operation()) {

			case File_system::Packet_descriptor::CONTENT_CHANGED:
				if (!(packet.handle() == _watch_handle))
					return false;

				if (_watching_file) {
					/* notify the client of the change */
					_curr_version = Version { _curr_version.value + 1 };
					_notify_client_about_new_version();
				} else {
					/* try and get closer to the file */
					_open_watch_handle();
				}
				return true;

			case File_system::Packet_descriptor::READ: {

				if (!(packet.handle() == _file_handle))
					return false;

				if (packet.position() > _file_seek || _file_seek >= _file_size) {
					error("bad packet seek position");
					_file_ds.realloc(&_env.ram(), 0);
					_file_seek = 0;
					return true;
				}

				size_t const n = min(packet.length(), _file_size - _file_seek);
				memcpy(_file_ds.local_addr<char>()+_file_seek,
				       _fs.tx()->packet_content(packet), n);
				_file_seek += n;
				return true;
			}

			case File_system::Packet_descriptor::WRITE:
				error("discarding strange WRITE acknowledgement");
				return true;
			case File_system::Packet_descriptor::SYNC:
				error("discarding strange SYNC acknowledgement");
				return true;
			case File_system::Packet_descriptor::READ_READY:
				error("discarding strange READ_READY acknowledgement");
				return true;
			}
			return false;
		}
};

struct Fs_rom::Packet_handler : Io_signal_handler<Packet_handler>
{
	Tx_source &source;

	/* list of open sessions */
	Sessions sessions { };

	void handle_packets()
	{
		while (source.ack_avail()) {
			File_system::Packet_descriptor pack = source.get_acked_packet();
			for (Rom_session_component *session = sessions.first();
			     session; session = session->next())
			{
				if (session->process_packet(pack))
					break;
			}
			source.release_packet(pack);
		}
	}

	Packet_handler(Entrypoint &ep, Tx_source &source)
	:
		Io_signal_handler<Packet_handler>(
			ep, *this, &Packet_handler::handle_packets),
		source(source)
	{ }
};


class Fs_rom::Rom_root : public Root_component<Fs_rom::Rom_session_component>
{
	private:

		Env          &_env;
		Heap          _heap { _env.ram(), _env.rm() };
		Allocator_avl _fs_tx_block_alloc { &_heap };

		/* open file-system session */
		File_system::Connection _fs { _env, _fs_tx_block_alloc };

		Packet_handler _packet_handler { _env.ep(), *_fs.tx() };

		Rom_session_component *_create_session(const char *args) override
		{
			Session_label const label = label_from_args(args);
			Session_label const module_name = label.last_element();

			/* create new session for the requested file */
			Rom_session_component *session = new (md_alloc())
				Rom_session_component(_env, _fs, module_name.string());

			_packet_handler.sessions.insert(session);
			return session;
		}

		void _destroy_session(Rom_session_component *session) override
		{
			_packet_handler.sessions.remove(session);
			Genode::destroy(md_alloc(), session);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param  env         Component environment
		 * \param  md_alloc    meta-data allocator used for ROM sessions
		 */
		Rom_root(Env       &env,
		         Allocator &md_alloc)
		:
			Root_component<Rom_session_component>(env.ep(), md_alloc),
			_env(env)
		{
			/* Process CONTENT_CHANGED acknowledgement packets at the entrypoint  */
			_fs.sigh_ack_avail(_packet_handler);

			env.parent().announce(env.ep().manage(*this));
		}
};


void Component::construct(Genode::Env &env)
{
	static Genode::Sliced_heap sliced_heap(env.ram(), env.rm());
	static Fs_rom::Rom_root inst(env, sliced_heap);
}
