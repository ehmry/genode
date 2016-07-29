/*
 * \brief  ROM component of VFS server
 * \author Emery Hemingway
 * \date   2016-05-14
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _VFS__ROM_COMPONENT_H_
#define _VFS__ROM_COMPONENT_H_

/* Genode includes */
#include <vfs/vfs_handle.h>
#include <vfs/dir_file_system.h>
#include <os/session_policy.h>
#include <rom_session/rom_session.h>
#include <root/component.h>
#include <base/rpc_server.h>

/* Local includes */
#include "types.h"


namespace Vfs_server {
	using namespace Vfs;
	class Rom_component;
	class Rom_root;
};


class Vfs_server::Rom_component :
	public Genode::Rpc_object<Genode::Rom_session>
{
	private:

		Path                          _root_path;
		Genode::Dataspace_capability  _ds_cap;
		char const                   *_path;
		Vfs::Directory_service       *_vfs;
		Genode::Env                  &_env;
		Genode::Allocator            &_alloc;
		Vfs::Vfs_handle              *_handle = nullptr;

		bool _open_handle()
		{
			if (_handle) return true;

			try {
				unsigned const mode = Vfs::Directory_service::OPEN_MODE_RDONLY;
				if (Vfs::Directory_service::Open_result::OPEN_OK ==
				    _vfs->open(_path, mode, &_handle, _alloc))
				{
					/* jump from the root to the leaf file system */
					_path = _vfs->leaf_path(_root_path.base());
					_vfs = &_handle->ds();
					return true;
				}
			} catch (Genode::Allocator::Out_of_memory) { }
			return false;
		}

		void _release()
		{
			if (_ds_cap.valid()) {
				_vfs->release(_path, _ds_cap);
				_ds_cap = Genode::Dataspace_capability();
			}
		}

	public:

		Rom_component(Vfs::Dir_file_system &vfs,
		              Genode::Env          &env,
		              Genode::Allocator    &alloc,
		              char           const *rom_path)
		:
			_root_path(rom_path), _path(_root_path.base()),
			_vfs(&vfs), _env(env), _alloc(alloc)
		{
			if (!vfs.leaf_path(_path)) {
				Genode::error("ROM lookup failed for '", _path, "'");
				throw Genode::Root::Unavailable();
			}
		}

		~Rom_component()
		{
			_release();

			if (_handle)
				_handle->ds().close(_handle);
		}


		/*******************
		 ** ROM interface **
		 *******************/

		Genode::Rom_dataspace_capability dataspace() override
		{
			using namespace Genode;

			if (!_open_handle())
				return Rom_dataspace_capability();

			_release();
			_ds_cap = _vfs->dataspace(_path);

			/*
			 * Take this opportunity to make an asynchronous resource request so
			 * the next client might not be blocked by a synchronous upgrade.
			 */
			if (_ds_cap.valid()) {
				size_t ds_size = Dataspace_client(_ds_cap).size();
				/*
				 * Try to keep at least as much free quota as
				 * the largest dataspace we have allocated.
				 */
				if (_env.ram().avail() < ds_size) {
					char arg_buf[32];
					snprintf(arg_buf, sizeof(arg_buf),
					         "ram_quota=%ld", ds_size);
					_env.parent().resource_request(arg_buf);
				}

				return static_cap_cast<Rom_dataspace>(_ds_cap);
			} else {
				log("failed to aquire dataspace for ", _path);
				return Rom_dataspace_capability();
			}
		}

		void sigh(Signal_context_capability sigh) override { }
};


class Vfs_server::Rom_root :
	public Genode::Root_component<Rom_component>
{
	private:

		Vfs::Dir_file_system &_vfs;
		Genode::Env          &_env;
		Genode::Heap          _heap { &_env.ram(), &_env.rm() };

		Genode::Attached_rom_dataspace const &_config;

	protected:

		Rom_component *_create_session(const char *args) override
		{
			using namespace Genode;

			Path session_root;

			Session_label const label = label_from_args(args);

			char tmp[MAX_PATH_LEN];
			try {
				Session_policy policy(label, _config.xml().sub_node("rom"));

				/* Determine the session root directory.
				 * Defaults to '/' if not specified by session
				 * policy or session arguments.
				 */
				try {
					policy.attribute("root").value(tmp, sizeof(tmp));
					session_root.import(tmp, "/");
				} catch (Xml_node::Nonexistent_attribute) { }

			} catch (...) { }

			session_root.append_element(label.last_element().string());

			Rom_component *session = new (md_alloc())
				Rom_component(_vfs, _env, _heap, session_root.base());
			log("ROM '", session_root, "' served to '", label, "'");
			return session;
		}

	public:

		Rom_root(Genode::Env                          &env,
		         Genode::Allocator                    &md_alloc,
		         Vfs::Dir_file_system                 &vfs,
		         Genode::Attached_rom_dataspace const &config)

		:
			Root_component<Rom_component>(&env.ep().rpc_ep(), &md_alloc),
			_vfs(vfs), _env(env), _config(config)
		{ }
};

#endif
