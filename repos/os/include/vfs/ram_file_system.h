/*
 * \brief  Embedded RAM VFS
 * \author Emery Hemingway
 * \date   2015-07-21
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__RAM_FILE_SYSTEM_H_
#define _INCLUDE__VFS__RAM_FILE_SYSTEM_H_

#include <ram_fs/chunk.h>
#include <vfs/file_system.h>
#include <base/allocator_guard.h>
#include <dataspace/client.h>
#include <util/list.h>

namespace Ram_fs {

	using namespace Genode;
	using namespace Vfs;

	class Node;
	class File;
	class Symlink;
	class Directory;

	enum { MAX_NAME_LEN = 128 };

	/**
	 * Return base-name portion of null-terminated path string
	 */
	static inline char const *basename(char const *path)
	{
		char const *start = path;

		for (; *path; ++path)
			if (*path == '/')
				start = path + 1;

		return start;
	}

}


class Ram_fs::Node : public List<Node>::Element
{
	private:

		char                _name[MAX_NAME_LEN];
		unsigned long const _inode;

		/**
		 * Generate unique inode number
		 *
		 * XXX: these inodes could clash with other VFS plugins
		 */
		static unsigned long _unique_inode()
		{
			static unsigned long inode_count;
			return ++inode_count;
		}

	public:

		Node() : _inode(_unique_inode()) { _name[0] = 0; }
		virtual ~Node() {}

		unsigned long inode() const { return _inode; }
		char    const *name() const { return _name; }

		/**
		 * Assign name
		 */
		void name(char const *name) { strncpy(_name, name, sizeof(_name)); }
};


class Ram_fs::File : public Node /* XXX: public Weak_object<Node> */
{
	private:

		typedef File_system::Chunk<4096>        Chunk_level_3;
		typedef File_system::Chunk_index<128, Chunk_level_3> Chunk_level_2;
		typedef File_system::Chunk_index<64,  Chunk_level_2> Chunk_level_1;
		typedef File_system::Chunk_index<64,  Chunk_level_1> Chunk_level_0;

		Chunk_level_0 _chunk;

		file_size _length;

	public:

		File(Allocator &alloc, char const *name)
		: _chunk(alloc, 0), _length(0) { Node::name(name); }

		size_t read(char *dst, size_t len, file_size seek_offset)
		{
			file_size const chunk_used_size = _chunk.used_size();

			if (seek_offset >= _length)
				return 0;

			/*
			 * Constrain read transaction to available chunk data
			 *
			 * Note that 'chunk_used_size' may be lower than '_length'
			 * because 'Chunk' may have truncated tailing zeros.
			 */
			if (seek_offset + len >= _length)
				len = _length - seek_offset;

			file_size read_len = len;

			if (seek_offset + read_len > chunk_used_size) {
				if (chunk_used_size >= seek_offset)
					read_len = chunk_used_size - seek_offset;
				else
					read_len = 0;
			}

			_chunk.read(dst, read_len, seek_offset);

			/* add zero padding if needed */
			if (read_len < len)
				memset(dst + read_len, 0, len - read_len);

			return len;
		}

		size_t write(char const *src, size_t len, file_size seek_offset)
		{
			if (seek_offset == (file_size)(~0))
				seek_offset = _chunk.used_size();

			if (seek_offset + len >= Chunk_level_0::SIZE)
				len = Chunk_level_0::SIZE - (seek_offset + len);

			_chunk.write(src, len, (size_t)seek_offset);

			/*
			 * Keep track of file length. We cannot use 'chunk.used_size()'
			 * as file length because trailing zeros may by represented
			 * by zero chunks, which do not contribute to 'used_size()'.
			 */
			_length = max(_length, seek_offset + len);

			return len;
		}

		file_size length() const { return _length; }

		void truncate(file_size size)
		{
			if (size < _chunk.used_size())
				_chunk.truncate(size);

			_length = size;
		}
};


class Ram_fs::Symlink : public Node
{
	private:

		char   _target[MAX_PATH_LEN];
		size_t _len;

	public:

		Symlink(char const *link_name, char const *target)
		{
			name(link_name);
			strncpy(_target, target, sizeof(_target));
			_len = strlen(_target);
		}

		size_t length() const { return _len; }

		size_t read(char *buf, file_size buf_size) const
		{
			size_t out = min(buf_size, _len);
			memcpy(buf, _target, out);
			return out;
		}
};


class Ram_fs::Directory : public Vfs::File_system, public Node
{
	private:

		class Ram_vfs_handle : public Vfs_handle
		{
			private:

				File *_file;

			public:
 
				Ram_vfs_handle(File_system &fs, int status_flags, File *file)
				: Vfs_handle(fs, fs, status_flags), _file(file)
				{ }
 
				File *file() const { return _file; }
		};

		List<Node>       _entries;
		size_t           _num_entries;
		Allocator_guard &_alloc;
		Lock             _lock;


		inline void insert(Node *node)
		{
			_entries.insert(node);
			++_num_entries;
		}

		inline void remove(Node *node)
		{
			_entries.remove(node);
			--_num_entries;
		}

		inline Node *lookup(char const *name)
		{
			for (Node *sub_node = _entries.first();
			     sub_node; sub_node = sub_node->next())
				if (strcmp(sub_node->name(), name) == 0)
					return sub_node;

			return nullptr;
		}

		inline Directory *lookup_dir(char const *name)
		{
			Node *node = lookup(name);
			if (node)
				return dynamic_cast<Directory *>(node);
			return nullptr;
		}

		inline File *lookup_file(char const *name)
		{
			Node *node = lookup(name);
			if (node)
				return dynamic_cast<File *>(node);
			return nullptr;
		}

		inline Symlink *lookup_symlink(char const *name)
		{
			Node *node = lookup(name);
			if (node)
				return dynamic_cast<Symlink *>(node);
			return nullptr;
		}

		/**
		 * Return the last directory on subpath.
		 */
		inline Directory *walk_path(char const *path)
		{
			if (*path != '/')
				return this;
			++path;

			Directory *dir = this;
			for (size_t i = 0; i < MAX_NAME_LEN; ++i) {
				if (!path[i]) {
					return dir;
				} else if (path[i] == '/') {
					char name[i];
					strncpy(name, path, ++i);
					path += i;
					i = 0;
					dir = dir->lookup_dir(name);
					if (!dir)
						return nullptr;
				}
			}
			return nullptr;
		}


		/*****************************************
		 ** Directory service (names not paths) **
		 *****************************************/ 

		Mkdir_result _mkdir(char const *name, unsigned mode)
		{
			Lock::Guard guard(_lock);
			if (strlen(name) >= MAX_NAME_LEN) {
				PERR("name to long");
				return MKDIR_ERR_NAME_TOO_LONG;
			}
			if (lookup(name)) {
				PERR("exists");
				return MKDIR_ERR_EXISTS;
			}

			Directory *dir = new (&_alloc) Directory(name, _alloc);
			insert(dir);
			return MKDIR_OK;
		}

		Open_result _open(char const *name, unsigned mode, Vfs_handle **handle)
		{
			Lock::Guard guard(_lock);
			File *file;

			if (mode & OPEN_MODE_CREATE) {
				if (strlen(name) >= MAX_NAME_LEN)
					return OPEN_ERR_NAME_TOO_LONG;
				if (lookup(name))
					return OPEN_ERR_EXISTS;

				file = new (&_alloc) File(_alloc, name);

				insert(file);

			} else {
				file = lookup_file(name);
				if (!file)
					return OPEN_ERR_UNACCESSIBLE;
			}

			*handle = new (&_alloc)
				Ram_vfs_handle(*this, mode, file);
			return OPEN_OK;
		}

		Stat_result _stat(char const *name, Stat &stat)
		{
			Lock::Guard guard(_lock);
			memset(&stat, 0x00, sizeof(stat));

			Node *node = lookup(name);
			if (!node)
				return STAT_ERR_NO_ENTRY;

			stat.inode = node->inode();

			File *file = dynamic_cast<File *>(node);
			if (file) {
				stat.size = file->length();
				stat.mode = STAT_MODE_FILE | 0777;
				return STAT_OK;
			}

			Directory *dir = dynamic_cast<Directory *>(node);
			if (dir) {
				stat.size = dir->_num_entries;
				stat.mode = STAT_MODE_DIRECTORY | 0777;
				return STAT_OK;
			}

			Symlink *symlink = dynamic_cast<Symlink *>(node);
			if (symlink) {
				stat.size = symlink->length();
				stat.mode = STAT_MODE_SYMLINK | 0777;
				return STAT_OK;
			}

			return STAT_ERR_NO_ENTRY;
		}

		Dirent_result _dirent(char const *name, file_offset index, Dirent &dirent)
		{
			Lock::Guard guard(_lock);

			if (index >= file_offset(_num_entries)) {
				PERR("index %llu is the end", index);
				dirent.fileno = _num_entries;
				dirent.type = DIRENT_TYPE_END;
				dirent.name[0] = '\0';
				return DIRENT_OK;
			}

			Node *node = _entries.first();
			++index;
			for (dirent.fileno = 1; dirent.fileno != index; ++dirent.fileno)
				node = node->next();

			if (dynamic_cast<File*>(node))
				dirent.type = DIRENT_TYPE_FILE;
			else if (dynamic_cast<Directory*>(node))
				dirent.type = DIRENT_TYPE_DIRECTORY;
			else if (dynamic_cast<Symlink*>(node))
				dirent.type = DIRENT_TYPE_SYMLINK;
			else
				dirent.type = DIRENT_TYPE_FILE;

			strncpy(dirent.name, node->name(), MAX_NAME_LEN);

			return DIRENT_OK;
		}

		Unlink_result _unlink(char const *node_name)
		{
			Lock::Guard guard(_lock);
			Node *node = lookup(node_name);
			if (!node)
				return UNLINK_ERR_NO_ENTRY;

			remove(node);

			/* XXX: reference count files */
			destroy(_alloc, node);
			return UNLINK_OK;
		}

		Symlink_result _symlink(char const *from_path, char const *link_name)
		{
			Lock::Guard guard(_lock);
			if ((strlen(from_path) >= MAX_PATH_LEN) ||
			    (strlen(link_name) >= MAX_NAME_LEN))
				return SYMLINK_ERR_NAME_TOO_LONG;

			if (lookup(link_name))
				return SYMLINK_ERR_EXISTS;

			Symlink *symlink = new (&_alloc) Symlink(link_name, from_path);
			insert(symlink);

			return SYMLINK_OK;
		}

		Readlink_result _readlink(char const *name, char *buf,
		                          file_size buf_size, file_size &out_len)
		{
			Lock::Guard guard(_lock);
			Symlink *symlink = lookup_symlink(name);
			if (!symlink)
				return READLINK_ERR_NO_ENTRY;

			out_len = symlink->read(buf, buf_size);
			return READLINK_OK;
		}

		Dataspace_capability _dataspace(char const *name)
		{
			Lock::Guard guard(_lock);
			File *file = lookup_file(name);
			if (!file)
				return Dataspace_capability();

			size_t len = file->length();

			/*
			 * Withdraw quota from the allocator to prevent OOM.
			 */
			if (len > (_alloc.quota() - _alloc.consumed()))
				return Dataspace_capability();

			Ram_dataspace_capability ds_cap;
			char *local_addr = nullptr;
			try {
				_alloc.withdraw(len);
				ds_cap = env()->ram_session()->alloc(len);

				local_addr = env()->rm_session()->attach(ds_cap);
				file->read(local_addr, file->length(), 0);
				env()->rm_session()->detach(local_addr);

			} catch(...) {
				env()->rm_session()->detach(local_addr);
				env()->ram_session()->free(ds_cap);
				_alloc.upgrade(len);
				return Dataspace_capability();
			}
			return ds_cap;
		}

	public:

		/**
		 * Constructor
		 */
		Directory(char const *dir_name, Genode::Allocator_guard &alloc)
		: Node(), _num_entries(0), _alloc(alloc) { Node::name(dir_name); }

		~Directory()
		{
			for (Node *node = _entries.first(); node; node = node->next())
				destroy(_alloc, node);
		}

		/*********************************
		 ** Directory service interface **
		 *********************************/

		Mkdir_result mkdir(char const *path, unsigned mode) override
		{
			Directory *dir = walk_path(path);
			if (!dir)
				return MKDIR_ERR_NO_ENTRY;

			try {
				return dir->_mkdir(basename(path), mode);
			} catch (Allocator::Out_of_memory) {
				PERR("%s: ram quota exceeded", __func__);
				return MKDIR_ERR_NO_SPACE;
			}
		}

		Open_result open(char const *path, unsigned mode, Vfs_handle **handle) override
		{
			Directory *dir = walk_path(path);
			if (!dir)
				return OPEN_ERR_UNACCESSIBLE;
			try {
				return dir->_open(basename(path), mode, handle);
			} catch (Allocator::Out_of_memory) {
				PERR("%s: ram quota exceeded", __func__);
				return OPEN_ERR_NO_SPACE;
			}
		}

		Symlink_result symlink(char const *from_path, char const *to_path) override
		{
			Directory *dir = walk_path(to_path);
			if (!dir)
				return SYMLINK_ERR_NO_ENTRY;
			try {
				return dir->_symlink(from_path, basename(to_path));
			} catch (Allocator::Out_of_memory) {
				PERR("%s: ram quota exceeded", __func__);
				return SYMLINK_ERR_NO_SPACE;
			}
		}

		Unlink_result unlink(char const *path) override
		{
			Directory *dir = walk_path(path);
			if (dir)
				return dir->_unlink(basename(path));
			return UNLINK_ERR_NO_ENTRY;
		}

		Stat_result stat(char const *path, Stat &stat) override
		{
			Directory *dir = walk_path(path);
			if (dir)
				return dir->_stat(basename(path), stat);
			return STAT_ERR_NO_ENTRY;
		}

		Dirent_result dirent(char const *path, file_offset index, Dirent &dirent) override
		{
			if (strcmp("/", path, 2) == 0)
				return _dirent(basename(path), index, dirent);

			Directory *dir = walk_path(path);
			if (dir) {
				dir = dir->lookup_dir(basename(path));
				if (dir) return dir->_dirent(basename(path), index, dirent);
			}
			return DIRENT_ERR_INVALID_PATH;
		}

		Readlink_result readlink(char const *path, char *buf,
		                         file_size buf_size, file_size &out_len) override
		{
			Directory *dir = walk_path(path);
			if (dir)
				return dir->_readlink(basename(path), buf, buf_size, out_len);
			return READLINK_ERR_NO_ENTRY;
		}

		Rename_result rename(char const *from, char const *to) override
		{
			Directory *from_dir = walk_path(from);
			Directory   *to_dir = walk_path(to);

			if (!(from_dir && to_dir))
				return RENAME_ERR_NO_ENTRY;

			char const *new_name = basename(to);
			Node *node = from_dir->lookup(basename(from));

			if (!node) return RENAME_ERR_NO_ENTRY;
			if (to_dir->lookup(new_name)) return RENAME_ERR_NO_PERM;

			from_dir->remove(node);
			node->name(new_name);
			to_dir->insert(node);

			return RENAME_OK;
		}

		file_size num_dirent(char const *path) override
		{
			if (strcmp("/", path, 2) == 0)
				return _num_entries;

			Directory *dir = walk_path(path);
			if (dir) {
				dir = dir->lookup_dir(basename(path));
				if (dir)
					return dir->_num_entries;
			}
			return 0;
		}

		bool is_directory(char const *path) override
		{
			if (strcmp("/", path, 2) == 0)
				return true;

			Directory *dir = walk_path(path);
			return (dir && dir->lookup_dir(basename(path)));
		}

		char const *leaf_path(char const *path) override
		{
			if (strcmp("/", path, 2) == 0)
				return path;

			Directory *dir = walk_path(path);
			if (dir && dir->lookup(basename(path)))
				return path;
			return nullptr;
		}

		Dataspace_capability dataspace(char const *path) override
		{
			Directory *dir = walk_path(path);
			if (dir)
				return dir->_dataspace(basename(path));
			return Dataspace_capability();
		}

		void release(char const *path, Dataspace_capability ds_cap) override
		{
			/* return quota removed from the allocator earlier */
			_alloc.upgrade(Dataspace_client(ds_cap).size());
			env()->ram_session()->free(static_cap_cast<Genode::Ram_dataspace>(ds_cap));
		}

		void register_read_ready_sigh(Vfs_handle *vfs_handle,
		                              Signal_context_capability sigh)
		{ }


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *vfs_handle,
		                   char const *buf, file_size buf_size,
		                   file_size &out_count) override
		{
			Ram_vfs_handle const *handle =
				static_cast<Ram_vfs_handle *>(vfs_handle);

			out_count = handle->file()->write(buf, buf_size, handle->seek());
			return WRITE_OK;
		}

		Read_result read(Vfs_handle *vfs_handle, char *dst, file_size count,
		                 file_size &out_count) override
		{
			Ram_vfs_handle const *handle =
				static_cast<Ram_vfs_handle *>(vfs_handle);

			out_count = handle->file()->read(dst, count, handle->seek());
			return READ_OK;
		}

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size len) override
		{
			Ram_vfs_handle const *handle =
				static_cast<Ram_vfs_handle *>(vfs_handle);

			if ((len > handle->file()->length()) &&
			    (len > (_alloc.quota()-_alloc.consumed())))
				return FTRUNCATE_ERR_NO_SPACE;

			handle->file()->truncate(len);
			return FTRUNCATE_OK;
		}

};

namespace Vfs { class Ram_file_system; }

struct Vfs::Ram_file_system : Ram_fs::Directory 
{

	Genode::Allocator_guard alloc;

	Ram_file_system(Xml_node config)
	: Directory("", alloc), alloc(env()->heap(), 0)
	{
		try {
			Genode::Number_of_bytes ram_bytes = 0;
			config.attribute("quota").value(&ram_bytes);
			alloc.upgrade(ram_bytes);
		} catch (...) {
			alloc.upgrade(env()->ram_session()->avail() - 1024*sizeof(long));
		}
	}

	/***************************
	 ** File_system interface **
	 ***************************/

	static char const *name() { return "ram"; }
};

#endif /* _INCLUDE__VFS__RAM_FILE_SYSTEM_H_ */
