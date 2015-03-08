/* Genode includes */
#include <root/component.h>
#include <os/server.h>
#include <nix_build_session/nix_build_session.h>
#include <base/printf.h>

/* Nix native includes */
#include <nix/main.h>

/* Nix includes */
#include <store.hh>

#include "session.h"


namespace Nichts_store {

	class Session_component;
	class    Root_component;
	class              Main;

	using namespace Genode;
};


class Nichts_store::Root_component : public Genode::Root_component<Worker>
{
	private:

		Ram_session_capability _ram;

		nix::Store _store;

	protected:

		/**
		 * Here the affinity is assumed to be the affinity that 
		 * the worker will use to execute builders. If that is
		 * not ideal, it can hopefully be fixed here and not
		 * at Worker.
		 */
		Worker *_create_session(const char *args, Affinity const &affinity) override
		{
			return new(md_alloc()) Worker(affinity, _ram, *md_alloc(), _store);
		}

		void _upgrade_session(Session_component *s, const char *args override
		{
			s->upgrade(args);
		}		

	public:

		/**
		 * Constructor
		 */
		Root_component(Server::Entrypoint &ep,
		               Genode::Allocator &md_alloc,
		               Ram_session_capability ram)
		:
			Genode::Root_component<Session_component>(&ep.rpc_ep(), &md_alloc),
			_ram(ram),
			_store(true)
		{
			env()->parent()->announce(ep.manage(nix_store_root));
		}
};


struct Nichts_store::Main
{
	Server::Entrypoint ep;

	Sliced_heap sliced_heap = { env()->ram_session(), env()->rm_session() };

	Root_component nix_store_root = { ep, sliced_heap,
	                                  env()->ram_session_cap() };

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		/* Initialise common configuration. */
		PLOG("initializing nix config...");
		nix::main::init();
	}
};


/************
 ** Server **
 ************/

namespace Server {

	char const *name() { return "nix_store_ep"; }

	size_t stack_size() { return 4*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Nichts_store::Main nix_store(ep);
	}

}