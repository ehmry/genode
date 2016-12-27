/*
 * \brief  Internal nodes of VFS server
 * \author Emery Hemingway
 * \date   2016-03-29
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _VFS__NODE_H_
#define _VFS__NODE_H_

/* Genode includes */
#include <vfs/file_system.h>
#include <file_system/node.h>
#include <file_system_session/file_system_session.h>
#include <os/path.h>

/* Local includes */
#include "assert.h"

namespace Vfs_server {

	using namespace File_system;
	using namespace Vfs;

	struct Node;
	struct Directory;
	struct File;
	struct Symlink;

	/* Vfs::MAX_PATH is shorter than File_system::MAX_PATH */
	enum { MAX_PATH_LEN = Vfs::MAX_PATH_LEN };

	typedef Genode::Path<MAX_PATH_LEN> Path;

	typedef Genode::Allocator::Out_of_memory Out_of_memory;

	/**
	 * Type trait for determining the node type for a given handle type
	 */
	template<typename T> struct Node_type;
	template<> struct Node_type<Node_handle>    { typedef Node      Type; };
	template<> struct Node_type<Dir_handle>     { typedef Directory Type; };
	template<> struct Node_type<File_handle>    { typedef File      Type; };
	template<> struct Node_type<Symlink_handle> { typedef Symlink   Type; };


	/**
	 * Type trait for determining the handle type for a given node type
	 */
	template<typename T> struct Handle_type;
	template<> struct Handle_type<Node>      { typedef Node_handle    Type; };
	template<> struct Handle_type<Directory> { typedef Dir_handle     Type; };
	template<> struct Handle_type<File>      { typedef File_handle    Type; };
	template<> struct Handle_type<Symlink>   { typedef Symlink_handle Type; };

	/*
	 * Note that the file objects are created at the
	 * VFS in the local node constructors, this is to
	 * ensure that Out_of_metadata is thrown before
	 * the VFS is modified.
	 */
}


struct Vfs_server::Node : File_system::Node_base
{
	Path const _path;
	Mode const  mode;

	Node(char const *node_path, Mode node_mode)
	: _path(node_path), mode(node_mode) { }

	virtual ~Node() { }

	char const *path() { return _path.base(); }

	virtual size_t read(Vfs::File_system&, char*, size_t, seek_off_t) { return 0; }
	virtual size_t write(Vfs::File_system&, char const*, size_t, seek_off_t) { return 0; }

};

struct Vfs_server::Symlink : Node
{
	Symlink(Vfs::File_system &vfs,
	        char       const *link_path,
	        Mode              mode,
	        bool              create)
	: Node(link_path, mode)
	{
		if (create)
			assert_symlink(vfs.symlink("", link_path));
	}


	/********************
	 ** Node interface **
	 ********************/

	size_t read(Vfs::File_system &vfs, char *dst, size_t len, seek_off_t seek_offset)
	{
		Vfs::file_size res = 0;
		return vfs.readlink(path(), dst, len, res) == Directory_service::READLINK_OK
			? res : 0;
	}

	size_t write(Vfs::File_system &vfs, char const *src, size_t len, seek_off_t seek_offset)
	{
		/* symlinks shall be atomic */
		if (seek_offset != 0) return 0;

		/* ensure symlink gets something null-terminated */
		Genode::String<MAX_PATH_LEN> target(Genode::Cstring(src, len));

		if (vfs.symlink(target.string(), path()) != Directory_service::SYMLINK_OK)
			return 0;

		return target.length()-1; /* do not count termination */
	}
};


class Vfs_server::File : public Node,
                         public Vfs::Read_callback,
                         public Vfs::Write_callback,
                         public Vfs::Notify_callback
{
	private:

		/* signal to notify client with */
		Genode::Signal_context_capability _sig_cap;

		::File_system::Session::Tx::Sink &_sink;

		Vfs::Vfs_handle *_handle;
		char const      *_leaf_path; /* offset pointer to Node::_path */

		/* a file can only have one pending packet op */
		::File_system::Packet_descriptor _packet;

		size_t _packet_offset;

	public:

		File(Vfs::File_system                 &vfs,
		     Genode::Allocator                &alloc,
		     char                       const *file_path,
		     ::File_system::Session::Tx::Sink &sink,
		     Mode                              fs_mode,
		     bool                              create)
		: Node(file_path, fs_mode), _sink(sink)
		{
			unsigned const vfs_mode =
				(fs_mode-1) | (create ? Vfs::Directory_service::OPEN_MODE_CREATE : 0);

			assert_open(vfs.open(file_path, vfs_mode, &_handle, alloc));
			_leaf_path = vfs.leaf_path(path());

			unsigned const acc_mode =
				vfs_mode & Vfs::Directory_service::OPEN_MODE_ACCMODE;

			if (acc_mode & Vfs::Directory_service::OPEN_MODE_WRONLY)
				_handle->write_callback(*this);

			if (acc_mode != Vfs::Directory_service::OPEN_MODE_WRONLY)
				_handle->read_callback(*this);

			/* XXX: just make sure this damn thing works */
			_handle->read_callback(*this);
			_handle->write_callback(*this);

		}

		~File()
		{
			_handle->ds().close(_handle);
			if (_packet.operation() != Packet_descriptor::INVALID) {
				_packet.length(0);
				_packet.result(Packet_descriptor::ERR_INVALID);
				_sink.acknowledge_packet(_packet);
			}
		}

		void truncate(file_size_t size) {
			assert_truncate(_handle->fs().ftruncate(_handle, size)); }

		unsigned poll() {
			return _handle->fs().poll(_handle); }

		void queue(::File_system::Packet_descriptor const &packet)
		{
			if (_packet.operation() != Packet_descriptor::INVALID) {
				Genode::warning("VFS server acking partial packet");
				/* push the old op */
				_packet.result(Packet_descriptor::ERR_INVALID);
				_sink.acknowledge_packet(_packet);
			}

			_packet = packet;
			_packet_offset = 0;
			_handle->seek(_packet.position());

			switch (_packet.operation()) {
			case Packet_descriptor::READ:
				_handle->fs().read(_handle, packet.length()); break;

			case Packet_descriptor::WRITE:
				_handle->fs().write(_handle, packet.length()); break;

			case Packet_descriptor::INVALID:
				_sink.acknowledge_packet(_packet); return;
			}
		}

		bool sigh(Genode::Signal_context_capability sig_cap)
		{

			if (_handle->ds().subscribe(_handle)) {
				_sig_cap = sig_cap;
				_handle->notify_callback(*this);
				return true;
			}
			return false;
		}


		/************************
		 ** Callback interface **
		 ************************/

		/**
		 * Read callback
		 */
		file_size read(char const *src, file_size src_len,
		               Callback::Status status) override
		{
			if (!(_packet.size() && _packet.operation() == Packet_descriptor::READ)) {
				Genode::error("read callback received for invalid packet");
				return 0;
			}

			char        *dst = _sink.packet_content(_packet)+_packet_offset;
			size_t const out = min(src_len, _packet.size()-_packet_offset);

			if (out) {
				if (src)
					Genode::memcpy(dst, src, out);
				else
					Genode::memset(dst, 0x00, out);
				_packet_offset += out;
			}

			typedef ::File_system::Packet_descriptor::Result Result;
			switch(status) {
			case Callback::COMPLETE:       _packet.result(Result::SUCCESS); break;
			case Callback::PARTIAL:        return out;

			case Callback::ERR_IO:         _packet.result(Result::ERR_IO);         break;
			case Callback::ERR_INVALID:    _packet.result(Result::ERR_INVALID);    break;
			case Callback::ERR_TERMINATED: _packet.result(Result::ERR_TERMINATED); break;
			default: break; /* Callback::PARTIAL is excluded */
			}

			_packet.length(_packet_offset);
			_sink.acknowledge_packet(_packet);
			_packet = Packet_descriptor();
			return out;
		}

		/**
		 * Write callback
		 */
		file_size write(char *dst, file_size dst_len,
		                Callback::Status status) override
		{
			file_size out = 0;
			if (!(_packet.size() && _packet.operation() == Packet_descriptor::WRITE)) {
				Genode::error("write callback received for invalid packet");
				return 0;
			}

			if (dst_len) {
				char   const *src = _sink.packet_content(_packet)+_packet_offset;
				out = min(dst_len, _packet.length()-_packet_offset);

				if (dst)
					Genode::memcpy(dst, src, out);

				_packet_offset += out;
			}

			typedef ::File_system::Packet_descriptor::Result Result;
			switch(status) {
			case Callback::COMPLETE:       _packet.result(Result::SUCCESS); break;
			case Callback::PARTIAL:        return out;

			case Callback::ERR_IO:         _packet.result(Result::ERR_IO);         break;
			case Callback::ERR_INVALID:    _packet.result(Result::ERR_INVALID);    break;
			case Callback::ERR_TERMINATED: _packet.result(Result::ERR_TERMINATED); break;
			default: break; /* Callback::PARTIAL is excluded */
			}

			_packet.length(_packet_offset);
			_sink.acknowledge_packet(_packet);
			_packet = Packet_descriptor();
			return out;
		}

		/**
		 * Notify callback
		 */
		void notify() override {
			Genode::Signal_transmitter(_sig_cap).submit(); };


		/********************
		 ** Node interface **
		 ********************/

		size_t read(Vfs::File_system&, char *dst, size_t len, seek_off_t seek_offset) { return 0; }

		size_t write(Vfs::File_system&, char const *src, size_t len, seek_off_t seek_offset) { return 0; }
};


struct Vfs_server::Directory : Node
{
	Directory(Vfs::File_system &vfs, char const *dir_path, bool create)
	: Node(dir_path, READ_ONLY)
	{
		if (create)
			assert_mkdir(vfs.mkdir(dir_path, 0));
	}

	File *file(Vfs::File_system  &vfs,
	           Genode::Allocator &alloc,
	           char        const *file_path,
	           ::File_system::Session::Tx::Sink &sink,
	           Mode               mode,
	           bool               create)
	{
		Path subpath(file_path, path());
		char const *path_str = subpath.base();

		File *file;
		try { file = new (alloc)
			File(vfs, alloc, path_str, sink, mode, create); }
		catch (Out_of_memory) { throw Out_of_metadata(); }
		return file;
	}

	Symlink *symlink(Vfs::File_system  &vfs,
	                 Genode::Allocator &alloc,
	                 char        const *link_path,
	                 Mode               mode,
	                 bool               create)
	{
		Path subpath(link_path, path());
		char const *path_str = subpath.base();

		if (!create) {
			Vfs::file_size out;
			assert_readlink(vfs.readlink(path_str, nullptr, 0, out));
		}

		Symlink *link;
		try { link = new (alloc) Symlink(vfs, path_str, mode, create); }
		catch (Out_of_memory) { throw Out_of_metadata(); }
		return link;
	}


	/********************
	 ** Node interface **
	 ********************/

	size_t read(Vfs::File_system &vfs, char *dst, size_t len, seek_off_t seek_offset)
	{
		Directory_service::Dirent vfs_dirent;
		size_t blocksize = sizeof(File_system::Directory_entry);

		unsigned index = (seek_offset / blocksize);

		size_t remains = len;

		while (remains >= blocksize) {
			if (vfs.dirent(path(), index++, vfs_dirent)
				!= Vfs::Directory_service::DIRENT_OK)
				return len - remains;

			File_system::Directory_entry *fs_dirent = (Directory_entry *)dst;
			fs_dirent->inode = vfs_dirent.fileno;
			switch (vfs_dirent.type) {
			case Vfs::Directory_service::DIRENT_TYPE_DIRECTORY:
				fs_dirent->type = File_system::Directory_entry::TYPE_DIRECTORY;
				break;
			case Vfs::Directory_service::DIRENT_TYPE_SYMLINK:
				fs_dirent->type = File_system::Directory_entry::TYPE_SYMLINK;
				break;
			case Vfs::Directory_service::DIRENT_TYPE_FILE:
			default:
				fs_dirent->type = File_system::Directory_entry::TYPE_FILE;
				break;
			}
			strncpy(fs_dirent->name, vfs_dirent.name, MAX_NAME_LEN);

			remains -= blocksize;
			dst += blocksize;
		}
		return len - remains;
	}
};

#endif /* _VFS__NODE_H_ */
