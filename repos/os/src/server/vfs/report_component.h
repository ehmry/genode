/*
 * \brief  Report component of VFS server
 * \author Emery Hemingway
 * \date   2016-05-18
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _VFS__REPORT_COMPONENT_H_
#define _VFS__REPORT_COMPONENT_H_

/* Genode includes */
#include <vfs/vfs_handle.h>
#include <vfs/dir_file_system.h>
#include <os/attached_ram_dataspace.h>
#include <report_session/report_session.h>
#include <os/session_policy.h>
#include <root/component.h>
#include <base/rpc_server.h>
#include <util/arg_string.h>

/* Local includes */
#include "types.h"

namespace Vfs_server {
	using namespace Vfs;
	class Report_component;
	class Report_root;
};


class Vfs_server::Report_component :
	public Genode::Rpc_object<Report::Session>
{
	private:

		Genode::Attached_ram_dataspace     _ds;
		Vfs::Vfs_handle                   *_handle;

		static void make_parent_dir(Vfs::Dir_file_system &vfs, char const *path)
		{
			typedef Vfs::Directory_service::Mkdir_result Result;

			Path parent(path);
			parent.strip_last_element();
			char const *parent_path = parent.base();

			switch (vfs.mkdir(parent_path, 0)) {
			case Result::MKDIR_OK:         return;
			case Result::MKDIR_ERR_EXISTS: return;
			case Result::MKDIR_ERR_NO_ENTRY:
				make_parent_dir(vfs, parent_path); break;
			default:
				Genode::error("failed to create report directory '", parent_path, "'");
				throw Genode::Root::Unavailable();
			}
			vfs.mkdir(parent_path, 0);
		}

	public:

		Report_component(Vfs::Dir_file_system &vfs,
		                 Genode::Env          &env,
		                 Genode::Allocator    &alloc,
		                 Genode::size_t const  buffer_size,
		                 char           const *report_path)
		:
			_ds(&env.ram(), buffer_size)
		{
			make_parent_dir(vfs, report_path);

			unsigned const mode = Vfs::Directory_service::OPEN_MODE_RDWR
			                    | Vfs::Directory_service::OPEN_MODE_CREATE;
			if (Vfs::Directory_service::Open_result::OPEN_OK !=
			    vfs.open(report_path, mode, &_handle, alloc))
			{
				unsigned const mode = Vfs::Directory_service::OPEN_MODE_RDWR;
				if (Vfs::Directory_service::Open_result::OPEN_OK !=
				    vfs.open(report_path, mode, &_handle, alloc))
				{
					Genode::error("failed to open report file '", report_path, "'");
					throw Genode::Root::Unavailable();
				}
				_handle->fs().ftruncate(_handle, 0);
			}

			Genode::log("Report session opened at '", report_path, "'");
		}

		~Report_component()
		{
			_handle->ds().close(_handle);
		}


		/**********************
		 ** Report interface **
		 **********************/

		Genode::Dataspace_capability dataspace() override { return _ds.cap(); }

		void submit(size_t length) override
		{
			Vfs::file_size out;
			_handle->fs().write(_handle, _ds.local_addr<char const>(), length, out);
		}

		void response_sigh(Signal_context_capability sigh) override { }

		Genode::size_t obtain_response() override
		{
			Vfs::file_size out;
			_handle->fs().read(_handle, _ds.local_addr<char>(), _ds.size(), out);
			return out;
		}
};


class Vfs_server::Report_root :
	public Genode::Root_component<Report_component>
{
	private:

		Vfs::Dir_file_system   &_vfs;
		Genode::Env            &_env;
		Genode::Heap            _heap { &_env.ram(), &_env.rm() };
		Genode::Attached_rom_dataspace const &_config;

		/**
		 * This function will eventually move to os/path.h
		 */
		static void path_from_label(Path &path, char const *label)
		{
			using namespace Genode;

			char tmp[MAX_PATH_LEN];
			size_t len = strlen(label);
		
			size_t i = 0;
			for (size_t j = 1; j < len; ++j) {
				if (!strcmp(" -> ", label+j, 4)) {
					path.append("/");
		
					strncpy(tmp, label+i, (j-i)+1);
					/* rewrite any directory seperators */
					for (size_t k = 0; k < MAX_PATH_LEN; ++k)
						if (tmp[k] == '/')
							tmp[k] = '_';
					path.append(tmp);
		
					j += 4;
					i = j;
				}
			}
			path.append("/");
			strncpy(tmp, label+i, MAX_PATH_LEN);
			/* rewrite any directory seperators */
			for (size_t k = 0; k < MAX_PATH_LEN; ++k)
				if (tmp[k] == '/')
					tmp[k] = '_';
			path.append(tmp);
		}

	protected:

		Report_component *_create_session(const char *args) override
		{
			using namespace Genode;

			Path session_path;

			Session_label const label = label_from_args(args);

			/*
			 * read buffer size and check quota
			 *
			 * Beware, the handle is not accounted for because
			 * the handle structure is internal to a VFS plugin.
			 */
			size_t const ram_quota =
				Arg_string::find_arg(args, "ram_quota").aligned_size();
			size_t const buffer_size =
				Arg_string::find_arg(args, "buffer_size").aligned_size();

			size_t const session_size =
				max((size_t)4096, sizeof(Report_component)) +
				buffer_size;

			if (ram_quota < session_size)
				throw Root::Quota_exceeded();

			char tmp[MAX_PATH_LEN];
			try {
				Session_policy policy(label, _config.xml().sub_node("report"));

				/* determine the session root directory */
				try {
					policy.attribute("root").value(tmp, sizeof(tmp));
					session_path.import(tmp, "/");
				} catch (Xml_node::Nonexistent_attribute) { }

			} catch (...) { }

			Path label_path;
			path_from_label(label_path, label.string());

			session_path.append(label_path.base());

			return new (md_alloc())
				Report_component(_vfs, _env, _heap, buffer_size, session_path.base());
		}

	public:

		Report_root(Genode::Env                          &env,
		            Genode::Allocator                    &md_alloc,
		            Vfs::Dir_file_system                 &vfs,
		            Genode::Attached_rom_dataspace const &config)
		:
			Root_component<Report_component>(&env.ep().rpc_ep(), &md_alloc),
			_vfs(vfs), _env(env), _config(config)
		{ }
};

#endif
