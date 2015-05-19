/*
 * \brief  Server that writes log messages to a file system.
 * \author Emery Hemingway
 * \date   2015-05-13
 *
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes. */
#include <os/log_file_session.h>
#include <file_system_session/connection.h>
#include <file_system/util.h>
#include <root/component.h>
#include <os/server.h>
#include <os/session_policy.h>
#include <base/printf.h>

namespace Log_file {

	using namespace Genode;

	class  Log_root_component;
	struct Main;

	enum { BLOCK_SIZE = 256 };

}

class Log_file::Log_root_component : public Genode::Root_component<Log_file::Session_component>
{
	private:

		Allocator_avl           _write_alloc;
		File_system::Connection _fs;

		File_system::File_handle open_file(File_system::Dir_handle &dir_handle,
		                                   char const *name)
		{
			try {
				return _fs.file(dir_handle, name, File_system::WRITE_ONLY, false);
			} catch (File_system::Lookup_failed) {
				return _fs.file(dir_handle, name, File_system::WRITE_ONLY, true);
			}
		}

	protected:

		Session_component *_create_session(const char *args)
		{
			using namespace File_system;

			/*
			 * TODO:
			 * A client registry for opened file handles.
			 * Without this two clients could write over each other
			 * as the offsets get out of sync.
			 */

			char path[MAX_PATH_LEN];
			path[0] = '/';
			char name[MAX_NAME_LEN];

			Dir_handle   dir_handle;
			File_handle file_handle;
			bool truncate = false;

			Session_label session_label(args);
			strncpy(path+1, session_label.string(), sizeof(path)-1);

			size_t len = strlen(path);
			size_t start = 1;
			for (size_t i = 1; i < len;) {
				if (strcmp(" -> ", path+i, 4) == 0) {
					path[i++] = '/';
					strncpy(path+i, path+i+3, sizeof(path)-i);
					start = i;
					i += 3;
				} else ++i;
			}

			snprintf(name, sizeof(name), "%s.log", path+start);
			path[(start == 1) ? start : start-1] = '\0';

			try {
				Session_policy policy(session_label);

				try {
					char  dirname[MAX_PATH_LEN-MAX_NAME_LEN];
					policy.attribute("dir").value(dirname, sizeof(dirname));
					if (dirname[0] != '/') {
						PERR("Session log directory `%s' is not absolute", dirname);
						throw Root::Unavailable();
					}
					dir_handle = ensure_dir(_fs, dirname);
				} catch (Xml_node::Nonexistent_attribute) { }

				try {
					policy.attribute("file").value(name, sizeof(name));
				} catch (Xml_node::Nonexistent_attribute) { }

				try {
					truncate = policy.attribute("truncate").has_value("yes");
				} catch (Xml_node::Nonexistent_attribute) { }

			} catch (Session_policy::No_policy_defined) { }

			if (!dir_handle.valid())
				dir_handle = ensure_dir(_fs, path);
			Handle_guard dir_guard(_fs, dir_handle);

			if (!file_handle.valid())
				file_handle = open_file(dir_handle, name);

			seek_off_t offset = 0;
			if (truncate)
				_fs.truncate(file_handle, 0);
			else
				offset = _fs.status(file_handle).size;

			return new (md_alloc()) Session_component(_fs, file_handle, offset);
		}

	public:

		/**
		 * Constructor
		 *
		 * TODO:
		 * Use all of the available ram for the transmission buffer.
		 * This means the amount of log buffering is determined by
		 * the quota received from the parent.
		 */
		Log_root_component(Server::Entrypoint &ep, Allocator &alloc)
		:
			Genode::Root_component<Session_component>(&ep.rpc_ep(), &alloc),
			_write_alloc(env()->heap()),
			_fs(_write_alloc, (env()->ram_session()->avail()
			                   - ((env()->ram_session()->avail() / BLOCK_SIZE) * _write_alloc.overhead(BLOCK_SIZE))))
		{
			PLOG("buffering up to %lu messages",
			     _fs.tx()->bulk_buffer_size() / BLOCK_SIZE);
		}

};

struct Log_file::Main
{
	Server::Entrypoint &ep;

	Sliced_heap sliced_heap = { env()->ram_session(), env()->rm_session() };

	Log_root_component root { ep, sliced_heap };

	Main(Server::Entrypoint &ep)
	: ep(ep)
	{
		Genode::env()->parent()->announce(ep.manage(root));
	}
};


/************
 ** Server **
 ************/

namespace Server {

	char const* name() { return "log_file_ep"; }

	size_t stack_size() { return 2*1024*sizeof(long); }

	void construct(Entrypoint &ep) { static Log_file::Main inst(ep); }

}