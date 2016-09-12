/*
 * \brief  File_system rpc server for foreign langauges
 * \author Emery Hemingway
 * \date   2016-09-11
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <file_system_session/rpc_object.h>
#include <os/static_root.h>

#include <gems/foreign_file_system.h>

namespace File_system {

	struct Session_component;
	struct Server;

};


struct File_system::Session_component : Session_rpc_object
{

	Session_component(Genode::Env &env)
	: Session_rpc_object(env.ram().alloc(4096), env.ep().rpc_ep()) { }

	File_handle file(Dir_handle parent, Name const &name, Mode mode, bool create) override
	{
		int err;
		File_handle handle = genode_fs_file(parent.value, name.string(), mode, create, &err);


		if (handle.valid())
			return handle;

		switch (err) {
		case INVALID_HANDLE:      throw Invalid_handle();
		case INVALID_NAME:        throw Invalid_name();
		case LOOKUP_FAILED:       throw Lookup_failed();
		case NODE_ALREADY_EXISTS: throw Node_already_exists();
		case NO_SPACE:            throw No_space();
		case OUT_OF_METADATA:     throw Out_of_metadata();
		default:                  throw Permission_denied();
		}
	}

	Symlink_handle symlink(Dir_handle parent, Name const &name, bool create) override
	{
		int err;
		Symlink_handle handle = genode_fs_symlink(parent.value, name.string(), create, &err);
		if (handle.valid())
			return handle;

		switch (err) {
		case INVALID_HANDLE:      throw Invalid_handle();
		case INVALID_NAME:        throw Invalid_handle();
		case LOOKUP_FAILED:       throw Lookup_failed();
		case NODE_ALREADY_EXISTS: throw Node_already_exists();
		case NO_SPACE:            throw No_space();
		case OUT_OF_METADATA:     throw Out_of_metadata();
		default:                  throw Permission_denied();
		}
	}

	Dir_handle dir(Path const &path, bool create) override
	{
		int err;
		Dir_handle handle = genode_fs_directory(path.string(), create, &err);
		if (handle.valid())
			return handle;

		switch (err) {
		case LOOKUP_FAILED:       throw Lookup_failed();
		case NAME_TOO_LONG:       throw Name_too_long();
		case NODE_ALREADY_EXISTS: throw Node_already_exists();
		case NO_SPACE:            throw No_space();
		case OUT_OF_METADATA:     throw Out_of_metadata();
		default:                  throw Permission_denied();
		}
	}

	/**
	 * Open existing node
	 *
	 * The returned node handle can be used merely as argument for
	 * 'status'.
	 *
	 * \throw Lookup_failed    path lookup failed because one element
	 *                         of 'path' does not exist
	 * \throw Out_of_metadata  server cannot allocate metadata
	 */
	Node_handle node(Path const &path) override
	{
		int err;
		Node_handle handle = genode_fs_node(path.string(), &err);
		if (handle.valid())
			return handle;

		switch (err) {
		case OUT_OF_METADATA: throw Out_of_metadata();
		default:              throw Lookup_failed();
		}
	}

	void close(File_system::Node_handle node)
	{
		 genode_fs_close(node.value);
	}

	File_system::Status status(File_system::Node_handle node)
	{
		Genode_fs_status st;
		genode_fs_status(node.value, &st);
		return { st.size, st.mode, st.inode };
	}

	void control(Node_handle, Control) override { }

	void unlink(Dir_handle parent, Name const &name) override
	{
		int err;
		if (genode_fs_unlink(parent.value, name.string(), &err) != 0)
			switch (err) {
			case INVALID_HANDLE: throw Invalid_handle();
			case INVALID_NAME:   throw Invalid_name();
			case LOOKUP_FAILED:  throw Lookup_failed();
			case NOT_EMPTY:      throw Not_empty();
			default:             throw Permission_denied();
			}
	}

	void truncate(File_handle file, file_size_t size) override
	{
		int err;
		if (genode_fs_resize(file.value, size, &err) != 0)
			switch (err) {
			case INVALID_HANDLE: throw Invalid_handle();
			case NO_SPACE:       throw No_space();
			default:             throw Permission_denied();
			}
	}

	/**
	 * Move and rename directory entry
	 *
	 * \throw Invalid_handle     a directory handle is invalid
	 * \throw Invalid_name       'to' contains invalid characters
	 * \throw Lookup_failed      'from' not found
	 * \throw Permission_denied  node modification not allowed
	 */
	void move(Dir_handle from_dir, Name const &from,
	          Dir_handle   to_dir, Name const &to) override
	{
		int err;
		if (genode_fs_move(from_dir.value, from.string(), to_dir.value, to.string(), &err) != 0)
			switch (err) {
			case INVALID_HANDLE: throw Invalid_handle();
			case INVALID_NAME:   throw Invalid_name();
			case LOOKUP_FAILED:  throw Lookup_failed();
			default:             throw Permission_denied();
			}
	}

	/** TODO */
	void sigh(Node_handle, Genode::Signal_context_capability sigh) override { }

	void sync(Node_handle node) { genode_fs_sync(node.value); }
};


struct File_system::Server
{
	Session_component session_component;

	Genode::Static_root<Session> root_component;

	Server(Genode::Env &env)
	:
		session_component(env),
		root_component(env.ep().manage(session_component))
	{ }
};

static Genode::Lazy_volatile_object<File_system::Server> server;

extern "C"
void genode_fs_init(void *env_ptr)
{
	Genode::Env *env = (Genode::Env*)env_ptr;
	if (!server.constructed())
		server.construct(*env);
}