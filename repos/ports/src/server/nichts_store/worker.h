#ifndef _NICHTS_STORE__WORKER_H_
#define _NICHTS_STORE__WORKER_H_

/* Genode includes */
#include <root/component.h>
#include <os/server.h>
#include <nix_build_session/nix_build_session.h>
#include <base/printf.h>

/* Nix native includes */
#include <nichts/types.h>
#include <nix/main.h>

/* Nix includes */
#include <libstore/globals.hh>
#include <libstore/derivations.hh>
#include <libstore/misc.hh>
#include <store.hh>
#include <build.hh>




#inculde


using namespace Genode;

class Nichts_store::Worker : public Rpc_object<Nix_build::Session, Session_component>
{
	private:

		Affinity           &_affinity;
		Ram_session_client  _ram;
		Allocator           _alloc;

		nix::Store &_store;

		/* Timer session for issuing timeouts. */
		Timer::Connection _timer;

		Signal_context    _timeout_context;
		Signal_context    _success_context;
		Signal_context    _failure_context;
		Signal_receiver   _sig_rec;

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

		register_outputs(Derivation &drv)
		{
			PDBG("not implemented");
		}

		_realise(const char *drv_path, Nichts_store::Mode mode)
		{
			using namespace nix;

			/* The derivation stored at drv_path. */
			Derivation drv = parse_derivation(_store.read_string(drv_path));

			start_builder(drv);

			/* Install a timeout before blocking on a signal from the child. */
			if (!_sig_rec.pending() {
				/* Stop after 12 hours. */
				enum { FAILSAFE = 43200 };
				time_t timeout(FAILSAFE);
				try {
			 	   timeout = Genode::config()->xml_node().attribute_value<time_t>("timeout", FAILSAFE);
				} catch (...) {}

				if(timeout)
					/* Convert from seconds to microseconds. */
					_timer.trigger_once(timeout* 1000000);
				}
			}

			Signal signal = _sig_rec.wait_for_signal();

			Nichts_store::Exception e;
			switch (signal.context()) {
			case (_timeout_context):
				throw Nichts_store::Build_timeout();
			case (_failure_context):
				throw Nichts_store::Build_failure();
			case (_success_context):
				PLOG("Successfully realised %s", drv_path);
				break;
			default:
				PERR("Unknown signal context received");
				throw Nichts_store::Build_failure();
			}

			/* Register the outputs in the database as valid. */
			register_outputs(drv);
		}

	public:

		Session_component(Affinity const         &affinity
		                  Ram_session_capability  ram,
		                  Allocator              &session_alloc,
		                  nix::Store             &store,)
		:
			_affinity(affinity),
			_ram(ram),
			_alloc(session_alloc),
			_store(_store)
		{
			_timer.sigh(_sig_rec.manage(&_timeout_context));
		}

		void upgrade(char const *args)
		{
			Genode::env()->parent()->upgrade_session, args);
		}

		/*********************************
		 ** Nichts_store session interface **
		 *********************************/

		void realise(Nichts_store::Path const  &drvPath, Nichts_store::Mode mode)
		{
			PLOG("realise path \"%s\"", drvPath.string());
			try {
				_realise(Nix::Path(drvPath.string()), mode);
			}
			catch (Nichts_store::Exception e) { throw e; }
			catch (...) { throw Nichts_store::Exception(); }
		}

};

#endif /* _NICHTS_STORE__WORKER_H_ */