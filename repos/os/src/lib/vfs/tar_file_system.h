/*
 * \brief  TAR file system
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__TAR_FILE_SYSTEM_H_
#define _INCLUDE__VFS__TAR_FILE_SYSTEM_H_

#include <rom_session/connection.h>
#include <vfs/file_system.h>
#include <vfs/vfs_handle.h>
#include <base/attached_rom_dataspace.h>

namespace Vfs { class Tar_file_system; }


class Vfs::Tar_file_system : public File_system
{
	Genode::Env       &_env;
	Genode::Allocator &_alloc;

	typedef Genode::String<64> Rom_name;
	Rom_name _rom_name;

	Genode::Attached_rom_dataspace _tar_ds { _env, _rom_name.string() };
	char                          *_tar_base = _tar_ds.local_addr<char>();
	file_size               const  _tar_size = _tar_ds.size();

	/*
	 * Noncopyable
	 */
	Tar_file_system(Tar_file_system const &);
	Tar_file_system &operator = (Tar_file_system const &);

	class Record
	{
		private:

			char _name[100];
			char _mode[8];
			char _uid[8];
			char _gid[8];
			char _size[12];
			char _mtime[12];
			char _checksum[8];
			char _type[1];
			char _linked_name[100];

			/**
			 * Convert ASCII-encoded octal number to unsigned value
			 */
			template <typename T>
			unsigned long _read(T const &field) const
			{
				/*
				 * Copy-out ASCII string to temporary buffer that is
				 * large enough to host an additional zero.
				 */
				char buf[sizeof(field) + 1];
				strncpy(buf, field, sizeof(buf));

				unsigned long value = 0;
				Genode::ascii_to_unsigned(buf, value, 8);
				return value;
			}

		public:

			/* length of on data block in tar */
			enum { BLOCK_LEN = 512 };

			/* record type values */
			enum { TYPE_FILE    = 0, TYPE_HARDLINK = 1,
			       TYPE_SYMLINK = 2, TYPE_DIR      = 5 };

			file_size          size() const { return _read(_size); }
			unsigned            uid() const { return _read(_uid);  }
			unsigned            gid() const { return _read(_gid);  }
			unsigned           mode() const { return _read(_mode); }
			unsigned           type() const { return _read(_type); }
			char const        *name() const { return _name;        }
			char const *linked_name() const { return _linked_name; }

			void *data() const { return (char *)this + BLOCK_LEN; }
	};

	class Node;

	class Tar_vfs_handle : public Vfs_handle
	{
		private:

			/*
			 * Noncopyable
			 */
			Tar_vfs_handle(Tar_vfs_handle const &);
			Tar_vfs_handle &operator = (Tar_vfs_handle const &);

		protected:

			Node const *_node;

		public:

			Tar_vfs_handle(File_system &fs, Allocator &alloc, int status_flags,
			               Node const *node)
			: Vfs_handle(fs, fs, alloc, status_flags), _node(node)
			{ }

			virtual Read_result read(char *dst, file_size count,
			                         file_size &out_count) = 0;
	};


	struct Tar_vfs_file_handle : Tar_vfs_handle
	{
		using Tar_vfs_handle::Tar_vfs_handle;

		Read_result read(char *dst, file_size count,
		                 file_size &out_count) override
		{
			file_size const record_size = _node->record->size();

			file_size const record_bytes_left = record_size >= seek()
			                                  ? record_size  - seek() : 0;

			count = min(record_bytes_left, count);

			char const *data = (char *)_node->record->data() + seek();

			memcpy(dst, data, count);

			out_count = count;
			return READ_OK;
		}
	};

	struct Tar_vfs_dir_handle : Tar_vfs_handle
	{
		using Tar_vfs_handle::Tar_vfs_handle;

		Read_result read(char *dst, file_size count,
		                 file_size &out_count) override
		{
			if (count < sizeof(Dirent))
				return READ_ERR_INVALID;

			Dirent *dirent = (Dirent*)dst;

			/* initialize */
			*dirent = Dirent();

			file_offset index = seek() / sizeof(Dirent);

			Node const *node = _node->lookup_child(index);

			if (!node)
				return READ_OK;

			dirent->fileno = (Genode::addr_t)node;

			Record const *record = node->record;

			while (record && (record->type() == Record::TYPE_HARDLINK)) {
				Tar_file_system &tar_fs = static_cast<Tar_file_system&>(fs());
				Node const *target = tar_fs.dereference(record->linked_name());
				record = target ? target->record : 0;
			}

			if (record) {
				switch (record->type()) {
				case Record::TYPE_FILE:
					dirent->type = DIRENT_TYPE_FILE;      break;
				case Record::TYPE_SYMLINK:
					dirent->type = DIRENT_TYPE_SYMLINK;   break;
				case Record::TYPE_DIR:
					dirent->type = DIRENT_TYPE_DIRECTORY; break;

				default:
					Genode::error("unhandled record type ", record->type(), " "
					              "for ", node->name);
				}
			} else {
				/* If no record exists, assume it is a directory */
				dirent->type = DIRENT_TYPE_DIRECTORY;
			}

			strncpy(dirent->name, node->name, sizeof(dirent->name));

			out_count = sizeof(Dirent);

			return READ_OK;
		}
	};

	struct Tar_vfs_symlink_handle : Tar_vfs_handle
	{
		using Tar_vfs_handle::Tar_vfs_handle;

		Read_result read(char *buf, file_size buf_size,
		                 file_size &out_count) override
		{
			Record const *record = _node->record;

			file_size const count = min(buf_size, 100ULL);

			memcpy(buf, record->linked_name(), count);

			out_count = count;

			return READ_OK;
		}
	};

	struct Scanner_policy_path_element
	{
		static bool identifier_char(char c, unsigned /* i */)
		{
			return (c != '/') && (c != 0);
		}
	};


	typedef Genode::Token<Scanner_policy_path_element> Path_element_token;


	struct Node : List<Node>, List<Node>::Element
	{
		char const *name;
		Record const *record;

		Node(char const *name, Record const *record) : name(name), record(record) { }

		Node *lookup(char const *name)
		{
			Absolute_path lookup_path(name);

			Node *parent_node = this;
			Node *child_node;

			Path_element_token t(lookup_path.base());

			while (t) {

				if (t.type() != Path_element_token::IDENT) {
						t = t.next();
						continue;
				}

				char path_element[MAX_PATH_LEN];

				t.string(path_element, sizeof(path_element));

				for (child_node = parent_node->first(); child_node; child_node = child_node->next()) {
					if (strcmp(child_node->name, path_element) == 0) {
						parent_node = child_node;
						break;
					}
				}

				if (!child_node)
					return 0;

				t = t.next();
			}

			return parent_node;
		}


		Node const *lookup_child(int index) const
		{
			for (Node const *child_node = first(); child_node; child_node = child_node->next(), index--) {
				if (index == 0)
					return child_node;
			}

			return 0;
		}


		file_size num_dirent()
		{
			file_size count = 0;
			for (Node *child_node = first(); child_node; child_node = child_node->next(), count++) ;
			return count;
		}

	} _root_node;


	/*
	 *  Create a Node for a tar record and insert it into the node list
	 */
	class Add_node_action
	{
		private:

			Genode::Allocator &_alloc;

			Node &_root_node;

		public:

			Add_node_action(Genode::Allocator &alloc,
			                Node              &root_node)
			: _alloc(alloc), _root_node(root_node) { }

			void operator()(Record const *record)
			{
				Absolute_path current_path(record->name());

				char path_element[MAX_PATH_LEN];

				Path_element_token t(current_path.base());

				Node *parent_node = &_root_node;
				Node *child_node;

				while(t) {

					if (t.type() != Path_element_token::IDENT) {
							t = t.next();
							continue;
					}

					Absolute_path remaining_path(t.start());

					t.string(path_element, sizeof(path_element));

					for (child_node = parent_node->first(); child_node; child_node = child_node->next()) {
						if (strcmp(child_node->name, path_element) == 0)
							break;
					}

					if (child_node) {

						if (remaining_path.has_single_element()) {
							/* Found a node for the record to be inserted.
							 * This is usually a directory node without
							 * record. */
							child_node->record = record;
						}
					} else {
						if (remaining_path.has_single_element()) {

							/*
							 * TODO: find 'path_element' in 'record->name'
							 * and use the location in the record as name
							 * pointer to save some memory
							 */
							Genode::size_t name_size = strlen(path_element) + 1;
							char *name = (char*)_alloc.alloc(name_size);
							strncpy(name, path_element, name_size);
							child_node = new (_alloc) Node(name, record);
						} else {

							/* create a directory node without record */
							Genode::size_t name_size = strlen(path_element) + 1;
							char *name = (char*)_alloc.alloc(name_size);
							strncpy(name, path_element, name_size);
							child_node = new (_alloc) Node(name, 0);
						}
						parent_node->insert(child_node);
					}

					parent_node = child_node;
					t = t.next();
				}
			}
	};


	template <typename Tar_record_action>
	void _for_each_tar_record_do(Tar_record_action tar_record_action)
	{
		/* measure size of archive in blocks */
		unsigned block_id = 0, block_cnt = _tar_size/Record::BLOCK_LEN;

		/* scan metablocks of archive */
		while (block_id < block_cnt) {

			Record *record = (Record *)(_tar_base + block_id*Record::BLOCK_LEN);

			tar_record_action(record);

			file_size size = record->size();

			/* some datablocks */       /* one metablock */
			block_id = block_id + (size / Record::BLOCK_LEN) + 1;

			/* round up */
			if (size % Record::BLOCK_LEN != 0) block_id++;

			/* check for end of tar archive */
			if (block_id*Record::BLOCK_LEN >= _tar_size)
				break;

			/* lookout for empty eof-blocks */
			if (*(_tar_base + (block_id*Record::BLOCK_LEN)) == 0x00)
				if (*(_tar_base + (block_id*Record::BLOCK_LEN + 1)) == 0x00)
					break;
		}
	}


	struct Num_dirent_cache
	{
		Lock             lock { };
		Node            &root_node;
		bool             valid;              /* true after first lookup */
		char             key[256];           /* key used for lookup */
		file_size        cached_num_dirent;  /* cached value */

		Num_dirent_cache(Node &root_node)
		: root_node(root_node), valid(false), cached_num_dirent(0) { }

		file_size num_dirent(char const *path)
		{
			Lock::Guard guard(lock);

			/* check for cache miss */
			if (!valid || strcmp(path, key) != 0) {
				Node *node = root_node.lookup(path);
				if (!node)
					return 0;
				strncpy(key, path, sizeof(key));
				cached_num_dirent = node->num_dirent();
				valid = true;
			}
			return cached_num_dirent;
		}
	} _cached_num_dirent;

	/**
	 * Walk hardlinks until we reach a file
	 */
	Node const *dereference(char const *path)
	{
		Node const *node = _root_node.lookup(path);
		Node const *slow_node = node;
		int i = 0;
		while (node) {
			Record const *record = node->record;
			if (!record || record->type() != Record::TYPE_HARDLINK)
				break; /* got it */

			/*
			 * The `node` pointer is followed every iteration and
			 * `slow_node` every-other iteration. If there is a
			 * loop then eventually we catch it as the faster
			 * laps the slower.
			 */
			node = _root_node.lookup(record->linked_name());
			if (i++ & 1) {
				slow_node = _root_node.lookup(slow_node->record->linked_name());
				if (node == slow_node) {
					Genode::error(_rom_name, " contains a hard-link loop at '", path, "'");
					node = nullptr;
				}
			}
		}
		return node;
	}

	public:

		Tar_file_system(Genode::Env       &env,
		                Genode::Allocator &alloc,
		                Genode::Xml_node   config,
		                Io_response_handler &)
		:
			_env(env), _alloc(alloc),
			_rom_name(config.attribute_value("name", Rom_name())),
			_root_node("", 0),
			_cached_num_dirent(_root_node)
		{
			Genode::log("tar archive '", _rom_name, "' "
			            "local at ", (void *)_tar_base, ", size is ", _tar_size);

			_for_each_tar_record_do(Add_node_action(_alloc, _root_node));
		}

		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Dataspace_capability dataspace(char const *path) override
		{
			Node const *node = dereference(path);
			if (!node || !node->record)
				return Dataspace_capability();

			Record const *record = node->record;
			if (record->type() != Record::TYPE_FILE) {
				Genode::error("TAR record \"", path, "\" has "
				              "unsupported type ", record->type());
				return Dataspace_capability();
			}

			try {
				Ram_dataspace_capability ds_cap =
					_env.ram().alloc(record->size());

				void *local_addr = _env.rm().attach(ds_cap);
				memcpy(local_addr, record->data(), record->size());
				_env.rm().detach(local_addr);

				return ds_cap;
			}
			catch (...) { Genode::warning(__func__, " could not create new dataspace"); }

			return Dataspace_capability();
		}

		void release(char const *, Dataspace_capability ds_cap) override
		{
			_env.ram().free(static_cap_cast<Genode::Ram_dataspace>(ds_cap));
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			out = Stat();

			Node const *node = dereference(path);
			if (!node)
				return STAT_ERR_NO_ENTRY;

			if (!node->record) {
				out.mode = STAT_MODE_DIRECTORY;
				return STAT_OK;
			}

			Record const *record = node->record;

			/* convert TAR record modes to stat modes */
			unsigned mode = record->mode();
			switch (record->type()) {
			case Record::TYPE_FILE:     mode |= STAT_MODE_FILE; break;
			case Record::TYPE_SYMLINK:  mode |= STAT_MODE_SYMLINK; break;
			case Record::TYPE_DIR:      mode |= STAT_MODE_DIRECTORY; break;

			default: break;
			}

			out.mode  = mode;
			out.size  = record->size();
			out.uid   = record->uid();
			out.gid   = record->gid();
			out.inode = (Genode::addr_t)node;
			out.device = (Genode::addr_t)this;

			return STAT_OK;
		}

		Unlink_result unlink(char const *path) override
		{
			Node const *node = dereference(path);
			if (!node)
				return UNLINK_ERR_NO_ENTRY;
			else
				return UNLINK_ERR_NO_PERM;
		}

		Rename_result rename(char const *from, char const *to) override
		{
			if (_root_node.lookup(from) || _root_node.lookup(to))
				return RENAME_ERR_NO_PERM;
			return RENAME_ERR_NO_ENTRY;
		}

		file_size num_dirent(char const *path) override
		{
			return _cached_num_dirent.num_dirent(path);
		}

		bool directory(char const *path) override
		{
			Node const *node = dereference(path);

			if (!node)
				return false;

			Record const *record = node->record;

			return record ? (record->type() == Record::TYPE_DIR) : true;
		}

		char const *leaf_path(char const *path) override
		{
			/*
			 * Check if path exists within the file system. If this is the
			 * case, return the whole path, which is relative to the root
			 * of this file system.
			 */
			Node *node = _root_node.lookup(path);
			return node ? path : 0;
		}

		Open_result open(char const *path, unsigned, Vfs_handle **out_handle,
		                 Genode::Allocator& alloc) override
		{
			Node const *node = dereference(path);
			if (!node || !node->record || node->record->type() != Record::TYPE_FILE)
				return OPEN_ERR_UNACCESSIBLE;

			*out_handle = new (alloc) Tar_vfs_file_handle(*this, alloc, 0, node);

			return OPEN_OK;
		}

		Opendir_result opendir(char const *path, bool /* create */,
		                       Vfs_handle **out_handle,
		                       Genode::Allocator& alloc) override
		{
			Node const *node = dereference(path);

			if (!node ||
			    (node->record && (node->record->type() != Record::TYPE_DIR)))
				return OPENDIR_ERR_LOOKUP_FAILED;

			*out_handle = new (alloc) Tar_vfs_dir_handle(*this, alloc, 0, node);

			return OPENDIR_OK;
		}

		Openlink_result openlink(char const *path, bool /* create */,
		                         Vfs_handle **out_handle, Allocator &alloc)
		{
			Node const *node = dereference(path);
			if (!node || !node->record ||
			    node->record->type() != Record::TYPE_SYMLINK)
				return OPENLINK_ERR_LOOKUP_FAILED;

			*out_handle = new (alloc) Tar_vfs_symlink_handle(*this, alloc, 0, node);

			return OPENLINK_OK;
		}

		void close(Vfs_handle *vfs_handle) override
		{
			Tar_vfs_handle *tar_handle =
				static_cast<Tar_vfs_handle *>(vfs_handle);

			if (tar_handle)
				destroy(vfs_handle->alloc(), tar_handle);
		}


		/***************************
		 ** File_system interface **
		 ***************************/

		static char const *name()   { return "tar"; }
		char const *type() override { return "tar"; }


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *, char const *, file_size,
		                   file_size &) override
		{
			return WRITE_ERR_INVALID;
		}

		Read_result complete_read(Vfs_handle *vfs_handle, char *dst,
		                          file_size count, file_size &out_count) override
		{
			out_count = 0;

			Tar_vfs_handle *handle = static_cast<Tar_vfs_handle *>(vfs_handle);

			if (!handle)
				return READ_ERR_INVALID;

			return handle->read(dst, count, out_count);
		}

		Ftruncate_result ftruncate(Vfs_handle *, file_size) override
		{
			return FTRUNCATE_ERR_NO_PERM;
		}

		bool read_ready(Vfs_handle *) override { return true; }
};

#endif /* _INCLUDE__VFS__TAR_FILE_SYSTEM_H_ */
