/*
 * \brief  VFS File_system server
 * \author Emery Hemingway
 * \date   2016-05-14
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _VFS__FS_COMPONENT_H_
#define _VFS__FS_COMPONENT_H_

/* Genode includes */
#include <vfs/dir_file_system.h>
#include <file_system_session/rpc_object.h>
#include <os/session_policy.h>
#include <ram_session/connection.h>
#include <root/component.h>
#include <util/arg_string.h>

/* Local includes */
#include "assert.h"
#include "node.h"
#include "types.h"

namespace Vfs_server {
	using namespace File_system;
	using namespace Vfs;
	class Fs_component;
	class Fs_root;
};


class Vfs_server::Fs_component :
	public File_system::Session_rpc_object
{
	private:

		enum { ROOT_HANDLE = 0 };

		/* maximum number of open nodes per session */
		enum { MAX_NODE_HANDLES = 128U };

		Node *_nodes[MAX_NODE_HANDLES];

		Genode::Env              &_env;
		Genode::String<160> const _label;
		Genode::Ram_connection    _ram   { _env, _label.string() };
		Genode::Heap              _alloc { _ram, _env.rm() };

		Genode::Signal_handler<Fs_component> _process_packet_dispatcher;

		Vfs::Dir_file_system      &_vfs;
		Directory                  _root;
		bool                       _writable;


		/****************************
		 ** Handle to node mapping **
		 ****************************/

		bool _in_range(int handle) const {
			return ((handle >= 0) && (handle < MAX_NODE_HANDLES));
		}

		int _next_slot()
		{
			for (int i = 1; i < MAX_NODE_HANDLES; ++i)
				if (_nodes[i] == nullptr)
					return i;

			throw Out_of_metadata();
		}

		/**
		 * Lookup node using its handle as key
		 */
		Node *_lookup_node(Node_handle handle) {
			return _in_range(handle.value) ? _nodes[handle.value] : 0; }

		/**
		 * Lookup typed node using its handle as key
		 *
		 * \throw Invalid_handle
		 */
		template <typename HANDLE_TYPE>
		typename Node_type<HANDLE_TYPE>::Type &_lookup(HANDLE_TYPE handle)
		{
			if (!_in_range(handle.value))
				throw Invalid_handle();

			typedef typename Node_type<HANDLE_TYPE>::Type Node;
			Node *node = dynamic_cast<Node *>(_nodes[handle.value]);
			if (!node)
				throw Invalid_handle();

			return *node;
		}

		bool _refer_to_same_node(Node_handle h1, Node_handle h2) const
		{
			if (!(_in_range(h1.value) && _in_range(h2.value)))
				throw Invalid_handle();

			return _nodes[h1.value] == _nodes[h2.value];
		}


		/******************************
		 ** Packet-stream processing **
		 ******************************/

		/**
		 * Perform packet operation
		 */
		void _process_packet_op(Packet_descriptor &packet)
		{
			void     * const content = tx_sink()->packet_content(packet);
			size_t     const length  = packet.length();
			seek_off_t const seek    = packet.position();

			if ((!(content && length)) || (packet.length() > packet.size())) {
				packet.succeeded(false);
				return;
			}

			/* resulting length */
			size_t res_length = 0;

			switch (packet.operation()) {

			case Packet_descriptor::READ: {
				Node *node = _lookup_node(packet.handle());
				if (!(node && (node->mode&READ_ONLY)))
					return;

				res_length = node->read(_vfs, (char *)content, length, seek);
				break;
			}

			case Packet_descriptor::WRITE: {
				Node *node = _lookup_node(packet.handle());
				if (!(node && (node->mode&WRITE_ONLY)))
					return;

				res_length = node->write(_vfs, (char const *)content, length, seek);
				break;
			}
			}

			packet.length(res_length);
			packet.succeeded(!!res_length);
		}

		void _process_packets()
		{
			while (tx_sink()->packet_avail() && tx_sink()->ready_to_ack()) {
				Packet_descriptor packet = tx_sink()->get_packet();

				/* assume failure by default */
				packet.succeeded(false);

				_process_packet_op(packet);

				tx_sink()->acknowledge_packet(packet);
			}
		}

		/**
		 * Check if string represents a valid path (must start with '/')
		 */
		static void _assert_valid_path(char const *path) {
			if (path[0] != '/') throw Lookup_failed(); }

		/**
		 * Check if string represents a valid name (must not contain '/')
		 */
		static void _assert_valid_name(char const *name)
		{
			if (!*name) throw Invalid_name();
			for (char const *p = name; *p; ++p)
				if (*p == '/')
					throw Invalid_name();
		}

	public:

		/**
		 * Constructor
		 * \param ep           thead entrypoint for session
		 * \param cache        node cache
		 * \param tx_buf_size  shared transmission buffer size
		 * \param root_path    path root of the session
		 * \param writable     whether the session can modify files
		 */
		Fs_component(Vfs::Dir_file_system &vfs,
		             Genode::Env          &env,
		             char          const *label,
		             size_t               ram_quota,
		             size_t               tx_buf_size,
		             char           const *root_path,
		             bool                  writable)
		:
			Session_rpc_object(env.ram().alloc(tx_buf_size), env.ep().rpc_ep()),
			_env(env),
			_label(label),
			_ram(/*env,*/ _label.string()),
			_process_packet_dispatcher(env.ep(), *this, &Fs_component::_process_packets),
			_vfs(vfs),
			_root(vfs, root_path, false),
			_writable(writable)
		{
			/*
			 * Register '_process_packets' dispatch function as signal
			 * handler for packet-avail and ready-to-ack signals.
			 */
			_tx.sigh_packet_avail(_process_packet_dispatcher);
			_tx.sigh_ready_to_ack(_process_packet_dispatcher);

			/*
			 * the '/' node is not dynamically allocated, so it is
			 * permanently bound to Dir_handle(0);
			 */
			_nodes[0] = &_root;
			for (unsigned i = 1; i < MAX_NODE_HANDLES; ++i)
				_nodes[i] = nullptr;

			_ram.ref_account(env.ram_session_cap());
			env.ram().transfer_quota(_ram.cap(), ram_quota);
		}

		/**
		 * Destructor
		 */
		~Fs_component()
		{
			Dataspace_capability ds = tx_sink()->dataspace();
			_env.ram().free(static_cap_cast<Genode::Ram_dataspace>(ds));
		}

		void upgrade(char const *args)
		{
			size_t new_quota =
				Genode::Arg_string::find_arg(args, "ram_quota").ulong_value(0);
			_env.ram().transfer_quota(_ram.cap(), new_quota);
		}


		/***************************
		 ** File_system interface **
		 ***************************/

		Dir_handle dir(File_system::Path const &path, bool create) override
		{
			if (create && (!_writable))
				throw Permission_denied();

			char const *path_str = path.string();
			/* '/' is bound to '0' */
			if (!strcmp(path_str, "/")) {
				if (create) throw Node_already_exists();
				return Dir_handle(0);
			}

			_assert_valid_path(path_str);
			Vfs_server::Path fullpath(_root.path());
			fullpath.append(path_str);
			path_str = fullpath.base();

			/* make sure a handle is free before allocating */
			auto slot = _next_slot();

			if (!create && !_vfs.is_directory(path_str))
				throw Lookup_failed();

			Directory *dir;
			try { dir = new (_alloc) Directory(_vfs, path_str, create); }
			catch (Out_of_memory) { throw Out_of_metadata(); }

			_nodes[slot] = dir;
			return Dir_handle(slot);
		}

		File_handle file(Dir_handle dir_handle, Name const &name,
		                 Mode fs_mode, bool create) override
		{
			if ((create || (fs_mode & WRITE_ONLY)) && (!_writable))
				throw Permission_denied();

			Directory &dir = _lookup(dir_handle);

			char const *name_str = name.string();
			_assert_valid_name(name_str);

			/* make sure a handle is free before allocating */
			auto slot = _next_slot();

			File *file = dir.file(_vfs, _alloc, name_str, fs_mode, create);

			_nodes[slot] = file;
			return File_handle(slot);
		}

		Symlink_handle symlink(Dir_handle dir_handle, Name const &name, bool create) override
		{
			if (create && !_writable) throw Permission_denied();

			Directory &dir = _lookup(dir_handle);

			char const *name_str = name.string();
			_assert_valid_name(name_str);

			/* make sure a handle is free before allocating */
			auto slot = _next_slot();

			Symlink *link = dir.symlink(_vfs, _alloc, name_str,
				_writable ? READ_WRITE : READ_ONLY, create);

			_nodes[slot] = link;
			return Symlink_handle(slot);
		}

		Node_handle node(File_system::Path const &path) override
		{
			char const *path_str = path.string();
			/* '/' is bound to '0' */
			if (!strcmp(path_str, "/"))
				return Node_handle(0);

			_assert_valid_path(path_str);

			/* re-root the path */
			Path sub_path(path_str+1, _root.path());
			path_str = sub_path.base();
			if (!_vfs.leaf_path(path_str))
				throw Lookup_failed();

			auto slot = _next_slot();
			Node *node;

			try { node  = new (_alloc) Node(path_str, STAT_ONLY); }
			catch (Out_of_memory) { throw Out_of_metadata(); }

			_nodes[slot] = node;
			return Node_handle(slot);
		}

		void close(Node_handle handle) override
		{
			/* handle '0' cannot be freed */
			if (!handle.value)
				return;

			if (!_in_range(handle.value))
				return;

			Node *node = _nodes[handle.value];
			if (!node) { return; }

			/*
			 * De-allocate handle
			 */

			if (File *file = dynamic_cast<File*>(node))
				destroy(_alloc, file);
			else if (Directory *dir = dynamic_cast<Directory*>(node))
				destroy(_alloc, dir);
			else if (Symlink *link = dynamic_cast<Symlink*>(node))
				destroy(_alloc, link);
			else
				destroy(_alloc, node);

			_nodes[handle.value] = 0;
		}

		Status status(Node_handle node_handle)
		{
			Directory_service::Stat vfs_stat;
			File_system::Status      fs_stat;

			Node &node = _lookup(node_handle);

			if (_vfs.stat(node.path(), vfs_stat) != Directory_service::STAT_OK) {
				memset(&fs_stat, 0x00, sizeof(fs_stat));
				return fs_stat;
			}

			fs_stat.inode = vfs_stat.inode;

			switch (vfs_stat.mode & (
				Directory_service::STAT_MODE_DIRECTORY |
				Directory_service::STAT_MODE_SYMLINK |
				File_system::Status::MODE_FILE)) {

			case Directory_service::STAT_MODE_DIRECTORY:
				fs_stat.mode = File_system::Status::MODE_DIRECTORY;
				fs_stat.size = _vfs.num_dirent(node.path()) * sizeof(Directory_entry);
				return fs_stat;

			case Directory_service::STAT_MODE_SYMLINK:
				fs_stat.mode = File_system::Status::MODE_SYMLINK;
				break;

			default: /* Directory_service::STAT_MODE_FILE */
				fs_stat.mode = File_system::Status::MODE_FILE;
				break;
			}

			fs_stat.size = vfs_stat.size;
			return fs_stat;
		}

		void unlink(Dir_handle dir_handle, Name const &name)
		{
			if (!_writable) throw Permission_denied();

			Directory &dir = _lookup(dir_handle);

			char const *name_str = name.string();
			_assert_valid_name(name_str);

			Path path(name_str, dir.path());

			assert_unlink(_vfs.unlink(path.base()));
		}

		void truncate(File_handle file_handle, file_size_t size)
		{
			_lookup(file_handle).truncate(size);
		}

		void move(Dir_handle from_dir_handle, Name const &from_name,
		          Dir_handle to_dir_handle,   Name const &to_name)
		{
			if (!_writable)
				throw Permission_denied();

			char const *from_str = from_name.string();
			char const   *to_str =   to_name.string();

			_assert_valid_name(from_str);
			_assert_valid_name(  to_str);

			Directory &from_dir = _lookup(from_dir_handle);
			Directory   &to_dir = _lookup(  to_dir_handle);

			Path from_path(from_str, from_dir.path());
			Path   to_path(  to_str,   to_dir.path());

			assert_rename(_vfs.rename(from_path.base(), to_path.base()));
		}

		void sigh(Node_handle handle, Signal_context_capability sigh) override { }

		void sync(Node_handle handle) override
		{
			if (_in_range(handle.value)) try {
				Node &node = _lookup(handle);
				_vfs.sync(node.path());
			} catch (Invalid_handle) { }

			_vfs.sync("/");
		}

		void control(Node_handle, Control) { }
};


class Vfs_server::Fs_root :
	public Genode::Root_component<Fs_component>
{
	private:

		Genode::Env          &_env;
		Vfs::Dir_file_system &_vfs;

		Genode::Attached_rom_dataspace const &_config;

		Fs_component *_session_from_policy(Genode::Session_label  const &label,
		                                   Genode::Session_policy const &policy,
		                                   char                   const *args)
		{
			using namespace Genode;

			Path session_root;
			bool writeable = false;

			char tmp[MAX_PATH_LEN];

			/* Determine the session root directory.
			 * Defaults to '/' if not specified by session
			 * policy or session arguments.
			 */
			try {
				policy.attribute("root").value(tmp, sizeof(tmp));
				session_root.import(tmp, "/");
			} catch (Xml_node::Nonexistent_attribute) { }

			/* Determine if the session is writeable.
			 * Policy overrides arguments, both default to false.
			 */
			if (policy.attribute_value("writeable", false))
				writeable = Arg_string::find_arg(args, "writeable").bool_value(false);

			Arg_string::find_arg(args, "root").string(tmp, sizeof(tmp), "/");
			if (Genode::strcmp("/", tmp, sizeof(tmp)))
				session_root.append_element(tmp);
			session_root.remove_trailing('/');

			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota").aligned_size();
			size_t tx_buf_size =
				Arg_string::find_arg(args, "tx_buf_size").aligned_size();

			if (!tx_buf_size)
				throw Root::Invalid_args();

			/*
			 * Check if donated ram quota suffices for session data,
			 * and communication buffer.
			 */
			size_t session_size =
				max((size_t)4096, sizeof(Fs_component)) +
				tx_buf_size;

			if (session_size > ram_quota) {
				Genode::error("insufficient 'ram_quota' from ", label.string(),
				              ", got ", ram_quota, ", need ", session_size);
				throw Root::Quota_exceeded();
			}
			ram_quota -= session_size;

			/* check if the session root exists */
			if (!((session_root == "/") || _vfs.is_directory(session_root.base()))) {
				Genode::error("session root '", session_root.base(), "' not found for '", label.string(), "'");
				throw Root::Unavailable();
			}

			Fs_component *session = new(md_alloc())
				Fs_component(_vfs, _env,
				             label.string(),
				             ram_quota,
				             tx_buf_size,
				             session_root.base(),
				             writeable);

			Genode::log("session opened for '", label, "' at '", session_root, "'");
			return session;
		}

	protected:

		Fs_component *_create_session(const char *args) override
		{
			using namespace Genode;

			Session_label const label = label_from_args(args);

			Fs_component *session;

			try {
				/* lookup policies from a <file-system> node */
				Session_policy policy(label, _config.xml().sub_node("file_system"));
				session = _session_from_policy(label, policy, args);

			} catch (Xml_node::Nonexistent_sub_node) {
				try {
					/* lookup policies from the <config> node */
					Session_policy policy(label, _config.xml());
					session = _session_from_policy(label, policy, args);

				} catch (Session_policy::No_policy_defined) {
					error("no File_system policy defined for '", label, "'");
					throw Root::Unavailable();
				}

			}
			return session;
		}

		void _upgrade_session(Fs_component *session,
		                      char   const *args) override
		{
			session->upgrade(args);
		}

	public:

		Fs_root(Genode::Env                          &env,
		        Genode::Allocator                    &md_alloc,
		        Vfs::Dir_file_system                 &vfs,
		        Genode::Attached_rom_dataspace const &config)
		:
			Root_component<Fs_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _vfs(vfs), _config(config)
		{ }

};

#endif
