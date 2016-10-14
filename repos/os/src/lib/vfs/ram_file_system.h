/*
 * \brief  Embedded RAM VFS
 * \author Emery Hemingway
 * \date   2015-07-21
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__RAM_FILE_SYSTEM_H_
#define _INCLUDE__VFS__RAM_FILE_SYSTEM_H_

#include <ram_fs/chunk.h>
#include <vfs/file_system.h>
#include <dataspace/client.h>
#include <util/avl_tree.h>

namespace Vfs_ram {

	using namespace Genode;
	using namespace Vfs;

	class Node;
	class File;
	class Symlink;
	class Directory;

	struct Ram_handle;
	typedef Genode::List<Ram_handle> Ram_handles;

	enum { MAX_NAME_LEN = 128 };

	typedef Genode::Allocator::Out_of_memory Out_of_memory;

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

namespace Vfs { class Ram_file_system; }

struct Vfs_ram::Ram_handle final : Vfs_handle, Ram_handles::Element
{
	Vfs_ram::File &file;

	Ram_handle(Vfs::File_system &fs,
	           Allocator       &alloc,
	           int              status_flags,
	           Vfs_ram::File   &node)
	: Vfs_handle(fs, fs, alloc, status_flags), file(node)
	{ }
};


class Vfs_ram::Node : public Genode::Avl_node<Node>, public Genode::Lock
{
	private:

		char _name[MAX_NAME_LEN];

		/**
		 * Generate unique inode number
		 */
		static unsigned _unique_inode()
		{
			/* XXX: a leak of of how many files have been created */
			static unsigned long inode_count = 0;
			return ++inode_count;
		}

	public:

		unsigned inode;

		Node(char const *node_name)
		: inode(_unique_inode()) { name(node_name); }

		virtual ~Node() { }

		char const *name() { return _name; }
		void name(char const *name) { strncpy(_name, name, MAX_NAME_LEN); }

		virtual Vfs::file_size length() = 0;

		virtual void sync() { }

		/************************
		 ** Avl node interface **
		 ************************/

		bool higher(Node *c) { return (strcmp(c->_name, _name) > 0); }

		/**
		 * Find index N by walking down the tree N times,
		 * not the most efficient way to do this.
		 */
		Node *index(file_offset &i)
		{
			if (i-- == 0)
				return this;

			Node *n;

			n = child(LEFT);
			if (n)
				n = n->index(i);

			if (n) return n;

			n = child(RIGHT);
			if (n)
				n = n->index(i);

			return n;
		}

		Node *sibling(const char *name)
		{
			if (strcmp(name, _name) == 0) return this;

			Node *c =
				Avl_node<Node>::child(strcmp(name, _name) > 0);
			return c ? c->sibling(name) : 0;
		}

		struct Guard
		{
			Node *node;

			Guard(Node *guard_node) : node(guard_node) { node->lock(); }

			~Guard() { node->unlock(); }
		};

};


class Vfs_ram::File final : public Vfs_ram::Node
{
	private:

		typedef Ram_fs::Chunk<4096>      Chunk_level_3;
		typedef Ram_fs::Chunk_index<128, Chunk_level_3> Chunk_level_2;
		typedef Ram_fs::Chunk_index<64,  Chunk_level_2> Chunk_level_1;
		typedef Ram_fs::Chunk_index<64,  Chunk_level_1> Chunk_level_0;

		Chunk_level_0 _chunk;
		file_size     _length = 0;
		Ram_handles   _handles;
		bool          _dirty;

	public:

		File(char const *name, Allocator &alloc)
		: Node(name), _chunk(alloc, 0) { }

		bool opened() const { return _handles.first() != nullptr; }

		void insert_handle(Ram_handle *handle)
		{
			if (!_handles.first()) _dirty = false;
			_handles.insert(handle);
		}

		void remove_handle(Ram_handle *handle)
		{
			_handles.remove(handle);
		}

		void sync()
		{
			if (_dirty) {
				_dirty = false; /* callbacks may dirty the file */
				for (Ram_handle *h = _handles.first(); h; h = h->next())
					h->notify_callback();
			}
		}

		template <typename FUNC>
		void read(FUNC const &func, file_size len, file_size seek_offset)
		{
			file_size const chunk_used_size = _chunk.used_size();

			if (seek_offset >= _length)
				return;

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

			_chunk.read(func, read_len, seek_offset);
		}

		template <typename FUNC>
		void write(FUNC const &func, file_size len, file_size seek_offset)
		{
			if (seek_offset == (file_size)(~0))
				seek_offset = _chunk.used_size();

			if (seek_offset + len >= Chunk_level_0::SIZE)
				len = Chunk_level_0::SIZE - (seek_offset + len);

			try {
				if (seek_offset+len > _length) {
					/* lambda the lambda to keep track of the new write length */
					file_size remain = len;
					auto wrapped_func = [&] (char *dst, size_t dst_len) {
						size_t n = min(remain, dst_len);
						func(dst, n);
						remain -= n;
					};
					_chunk.write(wrapped_func, len, (size_t)seek_offset);
					len -= remain;
					_length = max(_length, seek_offset + len);
				} else
					_chunk.write(func, len, (size_t)seek_offset);

			} catch (Out_of_memory) { }

			_dirty = true;
		}

		file_size length() { return _length; }

		void truncate(file_size size)
		{
			if (size < _chunk.used_size())
				_chunk.truncate(size);

			_length = size;
			_dirty = true;
		}
};


class Vfs_ram::Symlink final : public Vfs_ram::Node
{
	private:

		char   _target[MAX_PATH_LEN];
		size_t _len = 0;

	public:

		Symlink(char const *name) : Node(name) { }

		file_size length() { return _len; }

		void set(char const *target, size_t len)
		{
			_len = min(len, MAX_PATH_LEN);
			memcpy(_target, target, _len);
		}

		size_t get(char *buf, size_t len)
		{
			size_t out = min(len, _len);
			memcpy(buf, _target, out);
			return out;
		}
};


class Vfs_ram::Directory final : public Vfs_ram::Node
{
	private:

		Avl_tree<Node>  _entries;
		file_size       _count = 0;

	public:

		Directory(char const *name)
		: Node(name) { }

		void empty(Allocator &alloc)
		{
			while (Node *node = _entries.first()) {
				_entries.remove(node);
				if (File *file = dynamic_cast<File*>(node)) {
					if (file->opened()) {
						file->inode = 0;
						continue;
					}
				} else if (Directory *dir = dynamic_cast<Directory*>(node)) {
					dir->empty(alloc);
				}
				destroy(alloc, node);
			}
		}

		void adopt(Node *node)
		{
			_entries.insert(node);
			++_count;
		}

		Node *child(char const *name)
		{
			Node *node = _entries.first();
			return node ? node->sibling(name) : 0;
		}

		void release(Node *node)
		{
			_entries.remove(node);
			--_count;
		}

		file_size length() override { return _count; }

		void dirent(file_offset index, Directory_service::Dirent &dirent)
		{
			Node *node = _entries.first();
			if (node) node = node->index(index);
			if (!node) {
				dirent.type = Directory_service::DIRENT_TYPE_END;
				return;
			}

			dirent.fileno = node->inode;
			strncpy(dirent.name, node->name(), sizeof(dirent.name));

			File *file = dynamic_cast<File *>(node);
			if (file) {
				dirent.type = Directory_service::DIRENT_TYPE_FILE;
				return;
			}

			Directory *dir = dynamic_cast<Directory *>(node);
			if (dir) {
				dirent.type = Directory_service::DIRENT_TYPE_DIRECTORY;
				return;
			}

			Symlink *symlink = dynamic_cast<Symlink *>(node);
			if (symlink) {
				dirent.type = Directory_service::DIRENT_TYPE_SYMLINK;
				return;
			}
		}

		void sync() override
		{
			Node *root_node = _entries.first();

			for (file_offset i = _count; i > 0; --i) {
				file_offset j = i; /* the index op overwrites j */
				if (Node *node = root_node->index(j))
					node->sync();
			}
		}
};


class Vfs::Ram_file_system final : public Vfs::File_system
{
	private:

		Genode::Env        &_env;
		Genode::Allocator  &_alloc;
		Vfs_ram::Directory  _root = { "" };

		Vfs_ram::Node *lookup(char const *path, bool return_parent = false)
		{
			using namespace Vfs_ram;

			if (*path ==  '/') ++path;
			if (*path == '\0') return &_root;

			char buf[Vfs::MAX_PATH_LEN];
			strncpy(buf, path, Vfs::MAX_PATH_LEN);
			Directory *dir = &_root;

			char *name = &buf[0];
			for (size_t i = 0; i < MAX_PATH_LEN; ++i) {
				if (buf[i] == '/') {
					buf[i] = '\0';

					Node *node = dir->child(name);
					if (!node) return 0;

					dir = dynamic_cast<Directory *>(node);
					if (!dir) return 0;

					/* set the current name aside */
					name = &buf[i+1];
				} else if (buf[i] == '\0') {
					if (return_parent)
						return dir;
					else
						return dir->child(name);
				}
			}
			return 0;
		}

		Vfs_ram::Directory *lookup_parent(char const *path)
		{
			using namespace Vfs_ram;

			Node *node = lookup(path, true);
			if (node)
				return dynamic_cast<Directory *>(node);
			return 0;
		}

		void remove(Vfs_ram::Node *node)
		{
			using namespace Vfs_ram;

			if (File *file = dynamic_cast<File*>(node)) {
				if (file->opened()) {
					file->inode = 0;
					return;
				}
			} else if (Directory *dir = dynamic_cast<Directory*>(node)) {
				dir->empty(_alloc);
			}

			destroy(_alloc, node);
		}

	public:

		Ram_file_system(Genode::Env       &env,
		                Genode::Allocator &alloc,
		                Genode::Xml_node)
		: _env(env), _alloc(alloc) { }

		~Ram_file_system() { _root.empty(_alloc); }


		/*********************************
		 ** Directory service interface **
		 *********************************/

		file_size num_dirent(char const *path) override
		{
			using namespace Vfs_ram;

			if (Node *node = lookup(path)) {
				Node::Guard guard(node);
				if (Directory *dir = dynamic_cast<Directory *>(node))
					return dir->length();
			}

			return 0;
		}

		bool directory(char const *path) override
		{
			using namespace Vfs_ram;

			Node *node = lookup(path);
			return node ? dynamic_cast<Directory *>(node) : 0;
		}

		char const *leaf_path(char const *path) {
			return lookup(path) ? path : 0; }

		Mkdir_result mkdir(char const *path, unsigned mode) override
		{
			using namespace Vfs_ram;

			Directory *parent = lookup_parent(path);
			if (!parent) return MKDIR_ERR_NO_ENTRY;
			Node::Guard guard(parent);

			char const *name = basename(path);

			if (strlen(name) >= MAX_NAME_LEN)
				return MKDIR_ERR_NAME_TOO_LONG;

			if (parent->child(name)) return MKDIR_ERR_EXISTS;

			try { parent->adopt(new (_alloc) Directory(name)); }
			catch (Out_of_memory) { return MKDIR_ERR_NO_SPACE; }

			return MKDIR_OK;
		}

		Open_result open(char const  *path, unsigned mode,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			using namespace Vfs_ram;

			File *file;
			char const *name = basename(path);

			if (mode & OPEN_MODE_CREATE) {
				Directory *parent = lookup_parent(path);
				Node::Guard guard(parent);

				if (!parent) return OPEN_ERR_UNACCESSIBLE;
				if (parent->child(name)) return OPEN_ERR_EXISTS;

				if (strlen(name) >= MAX_NAME_LEN)
					return OPEN_ERR_NAME_TOO_LONG;

				try { file = new (_alloc) File(name, _alloc); }
				catch (Out_of_memory) { return OPEN_ERR_NO_SPACE; }
				parent->adopt(file);
			} else {
				Node *node = lookup(path);
				if (!node) return OPEN_ERR_UNACCESSIBLE;

				file = dynamic_cast<File *>(node);
				if (!file) return OPEN_ERR_UNACCESSIBLE;
			}

			Ram_handle *handle = new (alloc)
				Ram_handle(*this, alloc, mode, *file);
			file->insert_handle(handle);
			*out_handle = handle;
			return OPEN_OK;
		}

		void close(Vfs_handle *vfs_handle) override
		{
			using namespace Vfs_ram;

			Ram_handle *ram_handle =
				static_cast<Ram_handle *>(vfs_handle);

			if (ram_handle) {
				File &file = ram_handle->file;
				file.remove_handle(ram_handle);
				destroy(vfs_handle->alloc(), ram_handle);

				if (!file.opened() && (file.inode == 0))
					/* the file was unlinked and is now no longer referenced */
					destroy(_alloc, &file);
				else
					/* notify if the file had been modified */
					file.sync();
			}
		}

		Stat_result stat(char const *path, Stat &stat) override
		{
			using namespace Vfs_ram;

			Node *node = lookup(path);
			if (!node) return STAT_ERR_NO_ENTRY;
			Node::Guard guard(node);

			stat.size  = node->length();
			stat.inode  = node->inode;
			stat.device = (Genode::addr_t)this;

			File *file = dynamic_cast<File *>(node);
			if (file) {
				stat.mode = STAT_MODE_FILE | 0777;
				return STAT_OK;
			}

			Directory *dir = dynamic_cast<Directory *>(node);
			if (dir) {
				stat.mode = STAT_MODE_DIRECTORY | 0777;
				return STAT_OK;
			}

			Symlink *symlink = dynamic_cast<Symlink *>(node);
			if (symlink) {
				stat.mode = STAT_MODE_SYMLINK | 0777;
				return STAT_OK;
			}

			/* this should never happen */
			return STAT_ERR_NO_ENTRY;
		}

		Dirent_result dirent(char const *path, file_offset index, Dirent &dirent) override
		{
			using namespace Vfs_ram;

			Node *node = lookup(path);
			if (!node) return DIRENT_ERR_INVALID_PATH;
			Node::Guard guard(node);

			Directory *dir = dynamic_cast<Directory *>(node);
			if (!dir) return DIRENT_ERR_INVALID_PATH;

			dir->dirent(index, dirent);
			return DIRENT_OK;
		}

		Symlink_result symlink(char const *target, char const *path) override
		{
			using namespace Vfs_ram;

			Symlink *link;
			Directory *parent = lookup_parent(path);
			if (!parent) return SYMLINK_ERR_NO_ENTRY;
			Node::Guard guard(parent);

			char const *name = basename(path);

			Node *node = parent->child(name);
			if (node) {
				node->lock();
				link = dynamic_cast<Symlink *>(node);
				if (!link) {
					node->unlock();
					return SYMLINK_ERR_EXISTS;
				}
			} else {
				if (strlen(name) >= MAX_NAME_LEN)
					return SYMLINK_ERR_NAME_TOO_LONG;

				try { link = new (_alloc) Symlink(name); }
				catch (Out_of_memory) { return SYMLINK_ERR_NO_SPACE; }

				link->lock();
				parent->adopt(link);
			}

			if (*target)
				link->set(target, strlen(target));
			link->unlock();
			return SYMLINK_OK;
		}

		Readlink_result readlink(char const *path, char *buf,
		                         file_size buf_size, file_size &out_len) override
		{
			using namespace Vfs_ram;
			Directory *parent = lookup_parent(path);
			if (!parent) return READLINK_ERR_NO_ENTRY;
			Node::Guard parent_guard(parent);

			Node *node = parent->child(basename(path));
			if (!node) return READLINK_ERR_NO_ENTRY;
			Node::Guard guard(node);

			Symlink *link = dynamic_cast<Symlink *>(node);
			if (!link) return READLINK_ERR_NO_ENTRY;

			out_len = link->get(buf, buf_size);
			return READLINK_OK;
		}

		Rename_result rename(char const *from, char const *to) override
		{
			using namespace Vfs_ram;

			if ((strcmp(from, to) == 0) && lookup(from))
				return RENAME_OK;

			char const *new_name = basename(to);
			if (strlen(new_name) >= MAX_NAME_LEN)
				return RENAME_ERR_NO_PERM;

			Directory *from_dir = lookup_parent(from);
			if (!from_dir) return RENAME_ERR_NO_ENTRY;
			Node::Guard from_guard(from_dir);

			Directory *to_dir = lookup_parent(to);
			if (!to_dir) return RENAME_ERR_NO_ENTRY;

			/* unlock the node so a second guard can be constructed */
			if (from_dir == to_dir)
				from_dir->unlock();

			Node::Guard to_guard(to_dir);

			Node *from_node = from_dir->child(basename(from));
			if (!from_node) return RENAME_ERR_NO_ENTRY;
			Node::Guard guard(from_node);

			Node *to_node = to_dir->child(new_name);
			if (to_node) {
				to_node->lock();

				if (Directory *dir = dynamic_cast<Directory*>(to_node))
					if (dir->length() || (!dynamic_cast<Directory*>(from_node)))
						return RENAME_ERR_NO_PERM;

				to_dir->release(to_node);
				remove(to_node);
			}

			from_dir->release(from_node);
			from_node->name(new_name);
			to_dir->adopt(from_node);

			return RENAME_OK;
		}

		Unlink_result unlink(char const *path) override
		{
			using namespace Vfs_ram;

			Directory *parent = lookup_parent(path);
			if (!parent) return UNLINK_ERR_NO_ENTRY;
			Node::Guard guard(parent);

			Node *node = parent->child(basename(path));
			if (!node) return UNLINK_ERR_NO_ENTRY;

			node->lock();
			parent->release(node);
			remove(node);
			return UNLINK_OK;
		}

		Dataspace_capability dataspace(char const *path) override
		{
			using namespace Vfs_ram;

			Ram_dataspace_capability ds_cap;

			Node *node = lookup(path);
			if (!node) return ds_cap;
			Node::Guard guard(node);

			File *file = dynamic_cast<File *>(node);
			if (!file) return ds_cap;

			size_t len = file->length();

			char *local_addr = nullptr;
			try {
				ds_cap = _env.ram().alloc(len);

				local_addr = _env.rm().attach(ds_cap);

				auto read_fn = [&] (char const *src, size_t src_len) {
					size_t n = min(len, src_len);
					memcpy(local_addr, src, n);
					len -= n;
					local_addr += n;
				};

				file->read(read_fn, file->length(), 0);
				_env.rm().detach(local_addr);

			} catch(...) {
				_env.rm().detach(local_addr);
				_env.ram().free(ds_cap);
				return Dataspace_capability();
			}
			return ds_cap;
		}

		void release(char const *path, Dataspace_capability ds_cap) override {
			_env.ram().free(
				static_cap_cast<Genode::Ram_dataspace>(ds_cap)); }

		void sync(char const *path) override
		{
			using namespace Vfs_ram;

			if (Node *node = lookup(path)) {
				Node::Guard guard(node);
				node->sync();
			}
		}

		bool subscribe(Vfs_handle*) override { return true; }


		/************************
		 ** File I/O interface **
		 ************************/

		void write(Vfs_handle *vfs_handle, file_size len) override
		{
			if ((vfs_handle->status_flags() & OPEN_MODE_ACCMODE) ==  OPEN_MODE_RDONLY)
				return vfs_handle->write_status(Callback::ERR_INVALID);

			Vfs_ram::Ram_handle const *handle =
				static_cast<Vfs_ram::Ram_handle *>(vfs_handle);
			Vfs_ram::Node::Guard guard(&handle->file);

			/* write_fn will be called for each chunk that the write spans */
			auto write_fn = [&] (char *dst, Genode::size_t dst_len) {
				vfs_handle->write_callback(dst, dst_len, Callback::PARTIAL);
			};
			handle->file.write(write_fn, len, handle->seek());

			/* XXX: out of space condition? */
			vfs_handle->write_status(Callback::COMPLETE);
		}

		void read(Vfs_handle *vfs_handle, file_size len) override
		{
			if ((vfs_handle->status_flags() & OPEN_MODE_ACCMODE) == OPEN_MODE_WRONLY)
				return vfs_handle->read_status(Callback::ERR_INVALID);

			Vfs_ram::Ram_handle const *handle =
				static_cast<Vfs_ram::Ram_handle *>(vfs_handle);
			Vfs_ram::Node::Guard guard(&handle->file);

			/* read_fn will be called for each chunk that the read spans */
			auto read_fn = [&] (char const *src, Genode::size_t src_len) {
				vfs_handle->read_callback(src, src_len, Callback::PARTIAL);
			};
			handle->file.read(read_fn, len, handle->seek());

			vfs_handle->read_status(Callback::COMPLETE);
		}

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size len) override
		{
			if ((vfs_handle->status_flags() & OPEN_MODE_ACCMODE) ==  OPEN_MODE_RDONLY)
				return FTRUNCATE_ERR_NO_PERM;

			Vfs_ram::Ram_handle const *handle =
				static_cast<Vfs_ram::Ram_handle *>(vfs_handle);

			Vfs_ram::Node::Guard guard(&handle->file);

			try { handle->file.truncate(len); }
			catch (Vfs_ram::Out_of_memory) { return FTRUNCATE_ERR_NO_SPACE; }
			return FTRUNCATE_OK;
		}


		/***************************
		 ** File_system interface **
		 ***************************/

		static char const *name() { return "ram"; }
};

#endif /* _INCLUDE__VFS__RAM_FILE_SYSTEM_H_ */
