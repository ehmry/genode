#ifndef _NICHTS_STORE__WORKER_H_
#define _NICHTS_STORE__WORKER_H_

/* Genode includes */
#include <base/affinity.h>
#include <base/signal.h>
#include <root/component.h>
#include <ram_session/client.h>
#include <timer_session/connection.h>
#include <file_system_session/connection.h>
#include <nichts_store_session/nichts_store_session.h>
#include <base/printf.h>
#include <os/config.h>
#include <os/server.h>

/* Nix native includes */
#include <nichts/types.h>
//#include <nix/main.h>

#include "derivation.h"

namespace Nichts_store {
	class Worker;
};

using namespace Genode;


/**
 * Return base-name portion of null-terminated path string
 */
static char const *basename(char const *path)
{
	char const *start = path;

	for (; *path; path++)
		if (*path == '/')
			start = path + 1;

	return start;
}


class Nichts_store::Worker : public Rpc_object<Nichts_store::Session, Worker>
{
	private:

		Affinity const           _affinity;
		Ram_session_client       _ram;
		//Allocator                _alloc;
		Genode::Allocator_avl    _fs_block_alloc;
		File_system::Connection  _fs;

		/* Timer session for issuing timeouts. */
		Timer::Connection _timer;

		Signal_context    _timeout_context;
		Signal_context    _success_context;
		Signal_context    _failure_context;
		Signal_receiver   _sig_rec;

		File_system::Dir_handle dir_of(const char *path, bool create)
		{
			using namespace File_system;
				int i;
				for (i = Genode::strlen(path); i != 0; --i)
					if (path[i] == '/')
						break;

				if (i) {
					char path_[++i];
					Genode::memcpy(path_, path, i-1);
					path_[i] = '\0';
					try {
						return _fs.dir(path_, create);
					}
					catch (Lookup_failed) {
						/* Apply this funtion to the parent and close the result. */
						_fs.close(dir_of(path_, true));
						return _fs.dir(path_, create);
					}
					catch (Node_already_exists) {
						return _fs.dir(path_, false);
					}
				}
				return _fs.dir("/", false);
		}

		/**
		 * Create a unique temporary directory using `name'
		 * as a base. Directories are rooted at /tmp.
		 */
		File_system::Path create_temp_dir(char const *name) {
			using namespace File_system;

			/* Create /tmp if it is missing. */
			try {
				Dir_handle tmp_dir = _fs.dir("/tmp", false);
				_fs.close(tmp_dir);
			}
			catch (Lookup_failed) {
				Dir_handle tmp_dir = _fs.dir("/tmp", true);
				_fs.close(tmp_dir);
			}

			/* Create a new and unique subdir. */
			int counter(1);
			char unique[File_system::Path::MAX_SIZE];
			while (1) {
				Genode::snprintf(&unique[0], sizeof(unique),
				                 "/tmp/%s-%d", name, counter);
				try {
					_fs.dir(unique, true);
					break;
				}
				catch (Node_already_exists) { };
			}
			return File_system::Path(unique);
		};

		std::string read_file(char const *path)
		{
			using namespace File_system;

			std::string str;
			File_handle file;
			File_system::Session::Tx::Source &source = *_fs.tx();
			File_system::Packet_descriptor packet_in, packet_out;

			try {
				Dir_handle dir = dir_of(path, false);
				file = _fs.file(dir, basename(path), READ_ONLY, false);
				_fs.close(dir);

				Status st = _fs.status(file);

				packet_in = File_system::Packet_descriptor(source.alloc_packet(st.size), 0, file,
	                                  File_system::Packet_descriptor::READ, st.size, 0);

				source.submit_packet(packet_in);

				packet_out = source.get_acked_packet();

				str.insert(0, source.packet_content(packet_out), packet_out.length());

			}
			catch (Genode::Exception &e) {
				source.release_packet(packet_out);
				source.release_packet(packet_in);
				_fs.close(file);
				throw e;
			}
			source.release_packet(packet_out);
			source.release_packet(packet_in);
			_fs.close(file);
			return str;
		};

		void start_builder_noux(Derivation &drv);

		void start_builder(Derivation &drv)
		{
			if (drv.platform.rfind("noux"))
				start_builder_noux(drv);
			else {
				PERR("building %s is unsupported", drv.platform.c_str());
				throw Nichts_store::Build_failure();
			}
		};

		void register_outputs(Derivation &drv)
		{
			PDBG("not implemented");
		}

		void _realise(const char *drv_path, Nichts_store::Mode mode)
		{
			/* The derivation stored at drv_path. */
			Derivation drv = parse_derivation(read_file(drv_path));

			start_builder(drv);

			/* Install a timeout before blocking on a signal from the child. */
			if (!_sig_rec.pending()) {
				/* Stop after 12 hours. */
				enum { FAILSAFE = 43200 };
				unsigned long timeout(FAILSAFE);
				try {
			 	   timeout = Genode::config()->xml_node().attribute_value<unsigned long>("timeout", FAILSAFE);
				} catch (...) {}

				if(timeout)
					/* Convert from seconds to microseconds. */
					_timer.trigger_once(timeout* 1000000);
			}

			Signal signal = _sig_rec.wait_for_signal();

			Signal_context *ctx = signal.context();

			if (ctx == &_success_context)
				PLOG("Successfully realised %s", drv_path);
			else if (ctx == &_failure_context)
				throw Nichts_store::Build_failure();
			else if (ctx == &_timeout_context)
				throw Nichts_store::Build_timeout();
			else {
				PERR("Unknown signal context received");
				throw Nichts_store::Build_failure();
			}

			/* Register the outputs in the database as valid. */
			//register_outputs(drv);
		}

	public:

		Worker(Affinity const         &affinity,
		       Ram_session_capability  ram,
		       Allocator              *session_alloc)
		:
			_affinity(affinity),
			_ram(ram),
			//_alloc(session_alloc),
			_fs_block_alloc(session_alloc),
			_fs(_fs_block_alloc)
		{
			_timer.sigh(_sig_rec.manage(&_timeout_context));
		}

		/*********************************
		 ** Nichts_store session interface **
		 *********************************/

		void realise(Nichts_store::Path const  &drvPath, Nichts_store::Mode mode)
		{
			PLOG("realise path \"%s\"", drvPath.string());
			try {
				_realise(drvPath.string(), mode);
			}
			catch (Nichts_store::Exception e) { throw e; }
			catch (...) { throw Nichts_store::Exception(); }
		}

};

#endif /* _NICHTS_STORE__WORKER_H_ */