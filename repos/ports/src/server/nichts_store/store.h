/*
 * \brief  Nichts_store session component
 * \author Emery Hemingway
 * \date   2015-03-13
 */

#ifndef _NICHTS_STORE__STORE_SESSION_H_
#define _NICHTS_STORE__STORE_SESSION_H_

/* Genode includes */
#include <base/snprintf.h>
#include <base/affinity.h>
#include <base/allocator_guard.h>
#include <base/printf.h>
#include <base/service.h>
#include <base/signal.h>
#include <ram_session/client.h>
#include <root/component.h>
#include <os/config.h>
#include <os/server.h>
#include <cap_session/cap_session.h>
#include <file_system_session/connection.h>
#include <timer_session/connection.h>
#include <nichts_store_session/nichts_store_session.h>

/* Local includes */
#include "builder.h"
#include "derivation.h"
#include "noux.h"


namespace Nichts_store {

	class Session_component;

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

	Genode::Dataspace_capability noux_ds_cap()
	{
		try {
			static Genode::Rom_connection rom("noux");
			static Genode::Dataspace_capability noux_ds = rom.dataspace();
			return noux_ds;
		} catch (...) { PERR("failed to obtain Noux binary dataspace"); }

		return Genode::Dataspace_capability();
	}

};

using namespace Genode;


class Nichts_store::Session_component : public Rpc_object<Nichts_store::Session, Nichts_store::Session_component>
{
	private:

		struct Label
		{
			char buf[sizeof("worker_x_y")];

			Label(Affinity const &affinity)
			{
				snprintf(buf, sizeof(buf), "worker_%d_%d",
				         affinity.location().xpos(),
				         affinity.location().ypos());
			}

		} _label;

		Signal_context    _timeout_context;
		Signal_context    _success_context;
		Signal_context    _failure_context;
		Signal_receiver   _sig_rec;

		/**
		 * Resources made available to builders.
		 */
		struct Resources
		{
			Affinity        affinity;
			Ram_connection  ram;
			Cpu_connection  cpu;
			Rm_connection   rm;

			Resources(char const *label, long priority,
			          Affinity const &aff, size_t ram_quota)
			:
				affinity(aff),
				ram(label),
				cpu(label, priority, affinity)
			{
				ram.ref_account(Genode::env()->ram_session_cap());
				Genode::env()->ram_session()->transfer_quota(ram.cap(), ram_quota);
			}

			void sigh(Signal_context_capability sig_cap)
			{
				/* register default exception handler */
				cpu.exception_handler(Thread_capability(), sig_cap);
			}

		} _resources;

		Genode::Allocator_guard  _session_alloc;

		Genode::Allocator_avl    _fs_block_alloc;
		File_system::Connection  _fs;
		Cap_session              *_cap;
		Service_registry        &_parent_services;

		/* Timer session for issuing timeouts. */
		Timer::Connection _timer;

	public:

		/**
		 * Constructor
		 */
		Session_component(long              priority,
		                  Affinity const   &affinity,
		                  Allocator        *session_alloc,
		                  size_t            ram_quota,
		                  Cap_session      *cap,
		                  Service_registry &parent_services)
		:
			_label(affinity),
			_resources(_label.buf, priority, affinity, ram_quota),
			_session_alloc(session_alloc, ram_quota),
			_fs_block_alloc(&_session_alloc),
			_fs(_fs_block_alloc),
			_cap(cap),
			_parent_services(parent_services)
		{
			_timer.sigh(_sig_rec.manage(&_timeout_context));

			/* Connecting this signal blocks the child from starting. */
			//_resources.sigh(_sig_rec.manage(&_failure_context));
		}

	private:

		/**
		 * Create a unique temporary directory using `name'
		 * as a base. Directories are rooted at /tmp.
		 */
		File_system::Path create_temp_dir(char const *name) {
			using namespace File_system;

			/* Create /tmp if it is missing. */
			try {
				Dir_handle tmp_dir = _fs.dir("/nix/tmp", false);
				_fs.close(tmp_dir);
			}
			catch (Lookup_failed) {
				Dir_handle tmp_dir = _fs.dir("/nix/tmp", true);
				_fs.close(tmp_dir);
			}

			/* Create a new and unique subdir. */
			int counter(1);
			char unique[File_system::MAX_PATH_LEN];
			while (1) {
				Genode::snprintf(&unique[0], sizeof(unique),
				                 "/nix/tmp/%s-%d", name, counter);
				try {
					_fs.dir(unique, true);
					break;
				}
				catch (Node_already_exists) { };
			}
			return File_system::Path(unique);
		};

		void register_outputs(Derivation &drv)
		{
			PDBG("not implemented");
		}

		void load_derivation(Derivation &drv, char const *path)
		{
			using namespace File_system;

			File_handle file;
			File_system::Session::Tx::Source &source = *_fs.tx();
			File_system::Packet_descriptor packet_in, packet_out;
			if (strcmp(path, "/nix/store", sizeof("/nix/store") -1)) {
				PERR("%s is not in the nix store!", path);
				throw Build_failure();
			}

			try {
				Dir_handle dir = _fs.dir("/nix/store", false);
				file = _fs.file(dir, basename(path), READ_ONLY, false);
				_fs.close(dir);

				Status st = _fs.status(file);

				packet_in = File_system::Packet_descriptor(source.alloc_packet(st.size), 0, file,
	                                  File_system::Packet_descriptor::READ, st.size, 0);

				source.submit_packet(packet_in);

				packet_out = source.get_acked_packet();

				drv.load(source.packet_content(packet_out), packet_out.length());

				source.release_packet(packet_out);
				source.release_packet(packet_in);
				_fs.close(file);
			}

			catch (File_system::Exception) {
				PERR("Failed to read file %s", path);
				source.release_packet(packet_out);
				source.release_packet(packet_in);
				_fs.close(file);
				throw Invalid_derivation();
			}
		};

		void _realise(const char *drv_path, Nichts_store::Mode mode)
		{
			/* The derivation stored at drv_path. */
			Derivation drv(&_session_alloc);
			load_derivation(drv, drv_path);

			enum { CONFIG_SIZE = 4096 }; /* This may not be big enough */
			Genode::Ram_dataspace_capability config_ds =
				_resources.ram.alloc(CONFIG_SIZE);

			/* Write a config for Noux to the dataspace. */
			noux_config(config_ds, CONFIG_SIZE, drv);

			Builder child(_label.buf, Native_pd_args(), _cap,
			              noux_ds_cap(), config_ds,
			              _sig_rec.manage(&_success_context),
			              _sig_rec.manage(&_failure_context),
			              _parent_services,
			              _resources.ram,
			              _resources.cpu);

			/* Install a timeout before blocking on a signal from the child. */
			if (!_sig_rec.pending()) {
				/* Stop after 12 hours. */
				enum { FAILSAFE = 43200 };
				unsigned long timeout(FAILSAFE);
				try {
					timeout = Genode::config()->xml_node().attribute_value<unsigned long>("timeout", FAILSAFE);
				} catch (...) {}

				if (timeout)
					/* Convert from seconds to microseconds. */
					_timer.trigger_once(timeout* 1000000);
			}

			PINF("waiting for signal...");
			Signal_context *ctx = _sig_rec.wait_for_signal().context();

			/* Free the config before handling the exit. */
			_resources.ram.free(config_ds);

			if (ctx == &_success_context)
				PLOG("Successfully realised %s", drv_path);
			else if (ctx == &_failure_context) {
				PERR("builder for %s failed", drv_path);
				throw Nichts_store::Build_failure();
			} else if (ctx == &_timeout_context) {
				PERR("builder for %s timed out", drv_path);
				throw Nichts_store::Build_timeout();
			} else {
				PERR("Unknown signal context received");
				throw Nichts_store::Build_failure();
			}

			/* Register the outputs in the database as valid. */
			//register_outputs(drv);
		}

	public:

		/************************************
		 ** Nichts_store session interface **
		 ************************************/

		File_system::Session_capability file_system_session()
		{
			PINF("%s received server side", __func__);
			return _fs;
		}

		Path hash(File_system::Node_handle node_handle)
		{
			Node *node = _sub_fs.lookup(node_handle);
			Node_lock_guard guard(*node);

			/*
			 * BLAKE and Skein have the highest software performance,
			 * yet the lowest hardware perfomance of the NIST finalists.
			 *
			 * The hashing is documented in nix/src/libstore/store-api.cc
			 */

			PINF("%s received server side", __func__);
			return node.hash();
		}

		void realise(Nichts_store::Path const  &drvPath, Nichts_store::Mode mode)
		{
			PLOG("realise path \"%s\"", drvPath.string());
			try {
				_realise(drvPath.string(), mode);
			}
			catch (Nichts_store::Exception) { throw Nichts_store::Build_failure(); }
			catch (...) { throw Nichts_store::Exception(); }
		}

};

#endif /* _NICHTS_STORE__STORE_SESSION_H_ */