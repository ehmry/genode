/*
 * \brief  Nichts_store server
 * \author Emery Hemingway
 * \date   2015-03-13
 */

/* Genode includes */
#include <base/service.h>
#include <root/component.h>
#include <os/server.h>
#include <util/arg_string.h>
#include <cap_session/connection.h>
#include <nichts_store_session/nichts_store_session.h>

/* Local includes */
#include "worker.h"


namespace Nichts_store {

	class Worker;
	class Root_component;
	class Main;

	using namespace Genode;
};


class Nichts_store::Root_component : public Genode::Root_component<Worker>
{
	private:

		Ram_session_capability _ram;
		Cap_connection         _cap;
		Service_registry       _parent_services;

	protected:

		/**
		 * Here the affinity is assumed to be the affinity that 
		 * the worker will use to execute builders. If that is
		 * not ideal, it can hopefully be fixed here and not
		 * at Worker.
		 */
		Worker *_create_session(const char *args, Affinity const &affinity) override
		{
			unsigned long prio = Arg_string::find_arg(args, "priority").long_value(0);
			size_t const ram_quota = Arg_string::find_arg(args, "ram_quota").ulong_value(0);

			return new(md_alloc()) Worker(prio, affinity, md_alloc(), ram_quota, &_cap, _parent_services);
		}		

	public:

		/**
		 * Constructor
		 */
		Root_component(Server::Entrypoint &ep,
		               Genode::Allocator &md_alloc,
		               Ram_session_capability ram)
		:
			Genode::Root_component<Worker>(&ep.rpc_ep(), &md_alloc),
			_ram(ram)
		{
			/*
			 * Whitelist of services that should be routed
			 * from our parent to our builders.
			 *
			 * TODO:
			 * Terminal is a service that should be managed by each worker.
			 */
			static char const *service_names[] = {
				"LOG", "ROM", "CAP", "CPU", "SIGNAL", "RAM", "RM", "PD",
				"Timer", "File_system", "Terminal", 0 };
			for (unsigned i = 0; service_names[i]; ++i)
				_parent_services.insert(new (Genode::env()->heap()) Genode::Parent_service(service_names[i]));

			env()->parent()->announce(ep.manage(*this));
		}
};


struct Nichts_store::Main
{
	Server::Entrypoint ep;

	Sliced_heap sliced_heap = { env()->ram_session(), env()->rm_session() };

	Root_component nix_store_root = { ep, sliced_heap,
	                                  env()->ram_session_cap() };

	Main(Server::Entrypoint &ep) : ep(ep) { }
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