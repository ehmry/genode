/*
 * \brief  Terminal that directs output to the LOG interface
 * \author Norman Feske
 * \date   2013-11-08
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/log_terminal_session.h>
#include <log_session/connection.h>
#include <root/component.h>
#include <os/server.h>

namespace Terminal {

	class Root_component;
	class Main;

	using namespace Genode;
};


class Terminal::Root_component : public Genode::Root_component<Session_component>
{
	private:

		Log_connection _log;

	protected:

		Session_component *_create_session(const char *args)
		{
			size_t const io_buffer_size = 4096;
			return new (md_alloc())
				Session_component(_log, io_buffer_size);
		}

	public:

		Root_component(Server::Entrypoint &ep, Genode::Allocator  &md_alloc)
		:
			Genode::Root_component<Session_component>(&ep.rpc_ep(), &md_alloc)
		{ }
};


struct Terminal::Main
{
	Server::Entrypoint &ep;

	Sliced_heap sliced_heap = { env()->ram_session(), env()->rm_session() };

	Root_component terminal_root = { ep, sliced_heap };

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		env()->parent()->announce(ep.manage(terminal_root));
	}
};


/************
 ** Server **
 ************/

namespace Server {

	char const *name() { return "log_terminal_ep"; }

	size_t stack_size() { return 4*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Terminal::Main terminal(ep);
	}
}
