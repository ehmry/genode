/*
 * \brief  Adapter from Genode 'File_system' session to VFS
 * \author Norman Feske
 * \author Emery Hemingway
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2012-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/*
 * The file system interface features asynchronous read and writes
 * using a shared packet buffer between the client and server. The
 * server may respond to packets out of order or not at all, so reads
 * and writes at this VFS plugin will always queue for callback or
 * cancelation.
 *
 * When a read or write length exceedes the amount of contiguous free
 * space in the shared buffer the operation will be truncated, it is
 * the callers responsibility to issue successive operations until
 * the desired amount of data has been processed.
 */

#ifndef _INCLUDE__VFS__FS_FILE_SYSTEM_H_
#define _INCLUDE__VFS__FS_FILE_SYSTEM_H_

/* Genode includes */
#include <vfs/file_system.h>
#include <file_system/util.h>
#include <file_system_session/connection.h>
#include <base/allocator_avl.h>
#include <util/avl_tree.h>
#include <base/log.h>

namespace Vfs { class Fs_file_system; }


class Vfs::Fs_file_system : public File_system
{
	private:

		enum { QUEUE_SIZE = ::File_system::Session::TX_QUEUE_SIZE*2 };

		typedef ::File_system::Packet_descriptor::Opcode Opcode;

		Genode::Env           &_env;
		Genode::Allocator_avl  _fs_packet_alloc;

		typedef Genode::String<64> Label_string;
		Label_string _label;

		typedef Genode::String<::File_system::MAX_NAME_LEN> Root_string;
		Root_string _root;

		::File_system::Connection _fs;


		/**
		 * Local plugin handle object
		 *
		 * Read and write operations that exceed the amount of contiguous
		 * free memory in the packet buffer are necessarily fragmented.
		 * This handle object tracks the number of outstanding bytes to
		 * complete the current handle I/O operation.
		 */
		struct Fs_vfs_handle : Vfs_handle, Genode::Avl_node<Fs_vfs_handle>
		{
			struct Notifier : Genode::Signal_handler<Notifier>
			{
				Fs_vfs_handle &handle;

				void callback() { handle.notify_callback(); }

				Notifier(Genode::Entrypoint &ep, Fs_vfs_handle &fs_h)
				:
					Genode::Signal_handler<Notifier>(ep, *this, &Notifier::callback),
					handle(fs_h)
				{ }
			};

			/* allocated by the handle allocator */
			Notifier *notifier = nullptr;

			::File_system::File_handle const file_handle;

			Fs_vfs_handle(File_system &fs, Allocator &alloc,
			              int status_flags, ::File_system::File_handle handle)
			: Vfs_handle(fs, fs, alloc, status_flags), file_handle(handle) { }

			Genode::Signal_context_capability sigh() const
			{
				return notifier ? *notifier : Genode::Signal_context_capability();
			}

			void drop_notifier()
			{
				if (notifier) {
					destroy(alloc(), notifier);
					notifier = nullptr;
				}
			}

			void alloc_notifier(Genode::Entrypoint &ep)
			{
				drop_notifier();
				notifier = new (alloc()) Notifier(ep, *this);
			}


			/************************
			 ** Avl node interface **
			 ************************/

			bool higher(Fs_vfs_handle *other) const {
				return (other->file_handle.value > file_handle.value); }

			Fs_vfs_handle *lookup(::File_system::Node_handle const handle)
			{
				if (handle == file_handle) return this;
				Fs_vfs_handle *h =
					Avl_node<Fs_vfs_handle>::child(handle.value > file_handle.value);
				return h ? h->lookup(handle) : nullptr;
			}
		};

		/* tree of handles with pending operations */
		Genode::Avl_tree<Fs_vfs_handle> _open_handles;

		/**
		 * Process an acknowledgement
		 */
		void _process_ack_packet(::File_system::Packet_descriptor const packet)
		{
			::File_system::Session::Tx::Source &source = *_fs.tx();

			Fs_vfs_handle *handle = _open_handles.first();
			handle = handle ? handle->lookup(packet.handle()) : nullptr;

			if (!handle) {
				Genode::warning("received acknowledgment for expired handle ", packet.handle().value);
				source.release_packet(packet);
				return;
			}

			Genode::size_t length = packet.length();

			Callback::Status s = Callback::ERR_INVALID;
			typedef ::File_system::Packet_descriptor::Result Result;
			switch(packet.result()) {
			case Result::SUCCESS:        s = Callback::COMPLETE;       break;
			case Result::ERR_IO:         s = Callback::ERR_IO;         break;
			case Result::ERR_INVALID:    s = Callback::ERR_INVALID;    break;
			case Result::ERR_TERMINATED: s = Callback::ERR_TERMINATED; break;
			}

			handle->seek(packet.position());

			char const *content = source.packet_content(packet);
			source.release_packet(packet);
			/* release packet now in case the callback jumps the stack */

			switch (packet.operation()) {
			case ::File_system::Packet_descriptor::READ:
				handle->read_callback(content, length, s);  break;
			case ::File_system::Packet_descriptor::WRITE:
				handle->write_status(s);      break;
			case ::File_system::Packet_descriptor::INVALID: break;
			}
		}

		/**
		 * Process callback and free acknowledgment packet
		 */
		void _handle_ack_packet()
		{
			::File_system::Session::Tx::Source &source = *_fs.tx();
			while(source.ack_avail())
				_process_ack_packet(source.get_acked_packet());
		}

		/**
		 * Queue a packet to the server
		 */
		bool _queue_io_packet(Fs_vfs_handle &handle, file_size count, Opcode op)
		{
			typedef ::File_system::Session::Tx::Source Source;
			Source &source = *_fs.tx();

			if (!source.ready_to_submit()) /* will not block */
				return 0;

			file_size op_len = min(count, source.bulk_buffer_size());

			::File_system::Packet_descriptor raw_packet;
			for (;;) {
				try {
					raw_packet = source.alloc_packet(op_len);
					break;
				} catch (Source::Packet_alloc_failed) {
					op_len /= 2;
					if (op_len < 2)
						return false;
				}
			}

			if (op_len < count)
				Genode::warning(count," byte operation fragmented to ", op_len);

			::File_system::Packet_descriptor
				packet(raw_packet, handle.file_handle,
				       op, op_len, handle.seek());

			if (op == Opcode::WRITE) {
				op_len = handle.write_callback(source.packet_content(packet),
				                               op_len, Callback::PARTIAL);
				if (!op_len) {
					source.release_packet(packet);
					return false;
				}
				packet.length(op_len);
			}

			source.submit_packet(packet);

			return true;
		}

		/**
		 * Process packets and callbacks when signaled by the server
		 */
		Genode::Signal_handler<Fs_file_system> _ack_handler
			{ _env.ep(), *this, &Fs_file_system::_handle_ack_packet };

	public:

		Fs_file_system(Genode::Env       &env,
		               Genode::Allocator &alloc,
		               Genode::Xml_node   config)
		:
			_env(env),
			_fs_packet_alloc(&alloc),
			_label(config.attribute_value("label", Label_string())),
			_root( config.attribute_value("root",  Root_string())),
			_fs(_fs_packet_alloc,
			    config.attribute_value("buffer_size",
				                       Genode::size_t(::File_system::DEFAULT_TX_BUF_SIZE)),
			    _label.string(), _root.string(),
			    config.attribute_value("writeable", true))
		{
			_fs.sigh_ack_avail(_ack_handler);
		}


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Dataspace_capability dataspace(char const *path) override
		{
			Absolute_path dir_path(path);
			dir_path.strip_last_element();

			Absolute_path file_name(path);
			file_name.keep_only_last_element();

			Ram_dataspace_capability ds_cap;
			char *local_addr = 0;

			try {
				::File_system::Dir_handle dir = _fs.dir(dir_path.base(),
				                                        false);
				::File_system::Handle_guard dir_guard(_fs, dir);

				::File_system::File_handle file =
				    _fs.file(dir, file_name.base() + 1,
				             ::File_system::READ_ONLY, false);
				::File_system::Handle_guard file_guard(_fs, file);

				::File_system::Status status = _fs.status(file);

				Ram_dataspace_capability ds_cap =
				    _env.ram().alloc(status.size);

				local_addr = _env.rm().attach(ds_cap);

				::File_system::Session::Tx::Source &source = *_fs.tx();
				file_size const max_packet_size = source.bulk_buffer_size() / 2;

				for (file_size seek_offset = 0; seek_offset < status.size;
				     seek_offset += max_packet_size) {

					file_size const count = min(max_packet_size, status.size -
					                                             seek_offset);

					::File_system::Packet_descriptor
						packet(source.alloc_packet(count),
							   file,
							   ::File_system::Packet_descriptor::READ,
							   count,
							   seek_offset);

					/* pass packet to server side */
					source.submit_packet(packet);

					for (;;) {
						packet = source.get_acked_packet();
						if (packet.handle() == file)
							break; /* this is our packet */

						/* not ours but it needs processing */
						_process_ack_packet(packet);
					}

					memcpy(local_addr + seek_offset, source.packet_content(packet), count);
					source.release_packet(packet);
				}

				_env.rm().detach(local_addr);

				return ds_cap;
			} catch(...) {
				_env.rm().detach(local_addr);
				_env.ram().free(ds_cap);
				return Dataspace_capability();
			}
		}

		void release(char const *path, Dataspace_capability ds_cap) override
		{
			if (ds_cap.valid())
				_env.ram().free(static_cap_cast<Genode::Ram_dataspace>(ds_cap));
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			::File_system::Status status;

			try {
				::File_system::Node_handle node = _fs.node(path);
				::File_system::Handle_guard node_guard(_fs, node);
				status = _fs.status(node);
			}
			catch (::File_system::Lookup_failed)   { return STAT_ERR_NO_ENTRY; }
			catch (::File_system::Out_of_metadata) { return STAT_ERR_NO_PERM;  }

			out = Stat();

			out.size = status.size;
			out.mode = STAT_MODE_FILE | 0777;

			if (status.symlink())
				out.mode = STAT_MODE_SYMLINK | 0777;

			if (status.directory())
				out.mode = STAT_MODE_DIRECTORY | 0777;

			out.uid = 0;
			out.gid = 0;
			out.inode = status.inode;
			out.device = (Genode::addr_t)this;
			return STAT_OK;
		}

		Dirent_result dirent(char const *path, file_offset index, Dirent &out) override
		{
			::File_system::Session::Tx::Source &source = *_fs.tx();

			if (strcmp(path, "") == 0)
				path = "/";

			::File_system::Dir_handle dir_handle;
			try { dir_handle = _fs.dir(path, false); }
			catch (::File_system::Lookup_failed) { return DIRENT_ERR_INVALID_PATH; }
			catch (::File_system::Name_too_long) { return DIRENT_ERR_INVALID_PATH; }
			catch (...) { return DIRENT_ERR_NO_PERM; }
			::File_system::Handle_guard dir_guard(_fs, dir_handle);

			enum { DIRENT_SIZE = sizeof(::File_system::Directory_entry) };

			::File_system::Packet_descriptor
				packet(source.alloc_packet(DIRENT_SIZE),
				       dir_handle,
				       ::File_system::Packet_descriptor::READ,
				       DIRENT_SIZE,
				       index*DIRENT_SIZE);

			/* pass packet to server side */
			source.submit_packet(packet);

			for (;;) {
				packet = source.get_acked_packet();
				if (packet.handle() == dir_handle)
					break; /* this is our packet */

				/* not ours but it needs processing */
				_process_ack_packet(packet);
			}

			typedef ::File_system::Directory_entry Directory_entry;

			/* copy-out payload into destination buffer */
			Directory_entry const *entry =
				(Directory_entry *)source.packet_content(packet);

			/*
			 * The default value has no meaning because the switch below
			 * assigns a value in each possible branch. But it is needed to
			 * keep the compiler happy.
			 */
			Dirent_type type = DIRENT_TYPE_END;

			switch (entry->type) {
			case Directory_entry::TYPE_DIRECTORY: type = DIRENT_TYPE_DIRECTORY; break;
			case Directory_entry::TYPE_FILE:      type = DIRENT_TYPE_FILE;      break;
			case Directory_entry::TYPE_SYMLINK:   type = DIRENT_TYPE_SYMLINK;   break;
			}

			out.fileno = entry->inode;
			out.type   = type;
			strncpy(out.name, entry->name, sizeof(out.name));

			source.release_packet(packet);

			return DIRENT_OK;
		}

		Unlink_result unlink(char const *path) override
		{
			Absolute_path dir_path(path);
			dir_path.strip_last_element();

			Absolute_path file_name(path);
			file_name.keep_only_last_element();

			try {
				::File_system::Dir_handle dir = _fs.dir(dir_path.base(), false);
				::File_system::Handle_guard dir_guard(_fs, dir);

				_fs.unlink(dir, file_name.base() + 1);
			}
			catch (::File_system::Invalid_handle)    { return UNLINK_ERR_NO_ENTRY;  }
			catch (::File_system::Invalid_name)      { return UNLINK_ERR_NO_ENTRY;  }
			catch (::File_system::Lookup_failed)     { return UNLINK_ERR_NO_ENTRY;  }
			catch (::File_system::Not_empty)         { return UNLINK_ERR_NOT_EMPTY; }
			catch (::File_system::Permission_denied) { return UNLINK_ERR_NO_PERM;   }

			return UNLINK_OK;
		}

		Readlink_result readlink(char const *path, char *buf, file_size buf_size,
		                         file_size &out_len) override
		{
			/* Canonicalize path (i.e., path must start with '/') */
			Absolute_path abs_path(path);
			abs_path.strip_last_element();

			Absolute_path symlink_name(path);
			symlink_name.keep_only_last_element();

			try {
				::File_system::Dir_handle dir_handle = _fs.dir(abs_path.base(), false);
				::File_system::Handle_guard from_dir_guard(_fs, dir_handle);

				::File_system::Symlink_handle symlink_handle =
					_fs.symlink(dir_handle, symlink_name.base() + 1, false);
				::File_system::Handle_guard symlink_guard(_fs, symlink_handle);

				/* read by packet */
				::File_system::Session::Tx::Source &source = *_fs.tx();

				::File_system::Packet_descriptor
					packet(source.alloc_packet(MAX_PATH_LEN), symlink_handle,
					       ::File_system::Packet_descriptor::READ,
					       MAX_PATH_LEN, 0);

				source.submit_packet(packet);

				for (;;) {
					packet = source.get_acked_packet();
					if (packet.handle() == symlink_handle)
						break; /* this is our packet */

					/* not ours but it needs processing */
					_process_ack_packet(packet);
				}

				out_len = min(packet.length(), buf_size);
				memcpy(buf, source.packet_content(packet), out_len);
				source.release_packet(packet);

				if (out_len < buf_size)
					buf[out_len] = '\0';

				return READLINK_OK;
			}
			catch (::File_system::Lookup_failed)  { return READLINK_ERR_NO_ENTRY; }
			catch (::File_system::Invalid_handle) { return READLINK_ERR_NO_ENTRY; }
			catch (...) { return READLINK_ERR_NO_PERM; }
		}

		Rename_result rename(char const *from_path, char const *to_path) override
		{
			if ((strcmp(from_path, to_path) == 0) && leaf_path(from_path))
				return RENAME_OK;

			Absolute_path from_dir_path(from_path);
			from_dir_path.strip_last_element();

			Absolute_path from_file_name(from_path);
			from_file_name.keep_only_last_element();

			Absolute_path to_dir_path(to_path);
			to_dir_path.strip_last_element();

			Absolute_path to_file_name(to_path);
			to_file_name.keep_only_last_element();

			try {
				::File_system::Dir_handle from_dir = _fs.dir(from_dir_path.base(), false);
				::File_system::Handle_guard from_dir_guard(_fs, from_dir);
				::File_system::Dir_handle to_dir = _fs.dir(to_dir_path.base(), false);
				::File_system::Handle_guard to_dir_guard(_fs, to_dir);

				_fs.move(from_dir, from_file_name.base() + 1,
				         to_dir,   to_file_name.base() + 1);
			}
			catch (::File_system::Lookup_failed) { return RENAME_ERR_NO_ENTRY; }
			catch (...)                          { return RENAME_ERR_NO_PERM; }

			return RENAME_OK;
		}

		Mkdir_result mkdir(char const *path, unsigned mode) override
		{
			/*
			 * Canonicalize path (i.e., path must start with '/')
			 */
			Absolute_path abs_path(path);

			try {
				_fs.close(_fs.dir(abs_path.base(), true));
			}
			catch (::File_system::Permission_denied)   { return MKDIR_ERR_NO_PERM; }
			catch (::File_system::Node_already_exists) { return MKDIR_ERR_EXISTS; }
			catch (::File_system::Lookup_failed)       { return MKDIR_ERR_NO_ENTRY; }
			catch (::File_system::Name_too_long)       { return MKDIR_ERR_NAME_TOO_LONG; }
			catch (::File_system::No_space)            { return MKDIR_ERR_NO_SPACE; }
			catch (::File_system::Out_of_metadata)     { return MKDIR_ERR_NO_ENTRY; }

			return MKDIR_OK;
		}

		Symlink_result symlink(char const *from, char const *to) override
		{
			/* Canonicalize path (i.e., path must start with '/') */
			Absolute_path abs_path(to);
			abs_path.strip_last_element();

			Absolute_path symlink_name(to);
			symlink_name.keep_only_last_element();

			unsigned const len = strlen(from);

			try {
				::File_system::Dir_handle dir_handle = _fs.dir(abs_path.base(), false);
				::File_system::Handle_guard from_dir_guard(_fs, dir_handle);

				::File_system::Symlink_handle symlink_handle =
					_fs.symlink(dir_handle, symlink_name.base() + 1, true);
				::File_system::Handle_guard symlink_guard(_fs, symlink_handle);

				if (len == 0)
					return SYMLINK_OK;

				/* write by packet */
				::File_system::Session::Tx::Source &source = *_fs.tx();

				::File_system::Packet_descriptor
					packet(source.alloc_packet(MAX_PATH_LEN), symlink_handle,
					       ::File_system::Packet_descriptor::WRITE,
					       len, 0);

				strncpy(source.packet_content(packet), from, len+1);
				source.submit_packet(packet);

				for (;;) {
					packet = source.get_acked_packet();
					if (packet.handle() == symlink_handle)
						break; /* this is our packet */

					/* not ours but it needs processing */
					_process_ack_packet(packet);
				}

				source.release_packet(packet);
			}
			catch (::File_system::Invalid_handle)      { return SYMLINK_ERR_NO_ENTRY; }
			catch (::File_system::Node_already_exists) { return SYMLINK_ERR_EXISTS;   }
			catch (::File_system::Invalid_name)        { return SYMLINK_ERR_NAME_TOO_LONG; }
			catch (::File_system::Lookup_failed)       { return SYMLINK_ERR_NO_ENTRY; }
			catch (::File_system::Permission_denied)   { return SYMLINK_ERR_NO_PERM;  }
			catch (::File_system::No_space)            { return SYMLINK_ERR_NO_SPACE; }
			catch (::File_system::Out_of_metadata)     { return SYMLINK_ERR_NO_ENTRY; }

			return SYMLINK_OK;
		}

		file_size num_dirent(char const *path) override
		{
			if (strcmp(path, "") == 0)
				path = "/";

			::File_system::Node_handle node;
			try { node = _fs.node(path); } catch (...) { return 0; }
			::File_system::Handle_guard node_guard(_fs, node);

			::File_system::Status status = _fs.status(node);

			return status.size / sizeof(::File_system::Directory_entry);
		}

		bool directory(char const *path) override
		{
			try {
				::File_system::Node_handle node = _fs.node(path);
				::File_system::Handle_guard node_guard(_fs, node);

				::File_system::Status status = _fs.status(node);

				return status.directory();
			}
			catch (...) { return false; }
		}

		char const *leaf_path(char const *path) override
		{
			/* check if node at path exists within file system */
			try {
				::File_system::Node_handle node = _fs.node(path);
				_fs.close(node);
			}
			catch (...) { return 0; }

			return path;
		}

		Open_result open(char const *path, unsigned vfs_mode, Vfs_handle **out_handle, Genode::Allocator& alloc) override
		{
			Absolute_path dir_path(path);
			dir_path.strip_last_element();

			Absolute_path file_name(path);
			file_name.keep_only_last_element();

			::File_system::Mode mode;

			switch (vfs_mode & OPEN_MODE_ACCMODE) {
			default:               mode = ::File_system::STAT_ONLY;  break;
			case OPEN_MODE_RDONLY: mode = ::File_system::READ_ONLY;  break;
			case OPEN_MODE_WRONLY: mode = ::File_system::WRITE_ONLY; break;
			case OPEN_MODE_RDWR:   mode = ::File_system::READ_WRITE; break;
			}

			bool const create = vfs_mode & OPEN_MODE_CREATE;

			try {
				::File_system::Dir_handle dir = _fs.dir(dir_path.base(), false);
				::File_system::Handle_guard dir_guard(_fs, dir);

				::File_system::File_handle file =
					_fs.file(dir, file_name.base() + 1, mode, create);

				Fs_vfs_handle *handle = new (alloc)
					Fs_vfs_handle(*this, alloc, vfs_mode, file);
				_open_handles.insert(handle);
				*out_handle = handle;
			}
			catch (::File_system::Lookup_failed)       { return OPEN_ERR_UNACCESSIBLE;  }
			catch (::File_system::Permission_denied)   { return OPEN_ERR_NO_PERM;       }
			catch (::File_system::Invalid_handle)      { return OPEN_ERR_UNACCESSIBLE;  }
			catch (::File_system::Node_already_exists) { return OPEN_ERR_EXISTS;        }
			catch (::File_system::Invalid_name)        { return OPEN_ERR_NAME_TOO_LONG; }
			catch (::File_system::Name_too_long)       { return OPEN_ERR_NAME_TOO_LONG; }
			catch (::File_system::No_space)            { return OPEN_ERR_NO_SPACE;      }
			catch (::File_system::Out_of_metadata)     { return OPEN_ERR_NO_PERM;       }

			return OPEN_OK;
		}

		void close(Vfs_handle *vfs_handle) override
		{
			if (!vfs_handle) return;

			Fs_vfs_handle *fs_handle = static_cast<Fs_vfs_handle *>(vfs_handle);

			if (fs_handle) {
				_fs.close(fs_handle->file_handle);

				_open_handles.remove(
					_open_handles.first()->lookup(fs_handle->file_handle));

				destroy(fs_handle->alloc(), fs_handle);
			}
		}

		void sync(char const *path) override
		{
			try {
				::File_system::Node_handle node = _fs.node(path);
				_fs.sync(node);
				_fs.close(node);
			} catch (...) { }
		}

		bool subscribe(Vfs_handle *vfs_handle) override
		{
			Fs_vfs_handle *fs_handle = static_cast<Fs_vfs_handle *>(vfs_handle);
			if (!fs_handle) return false;

			try {
				fs_handle->alloc_notifier(_env.ep());
				if (_fs.sigh(fs_handle->file_handle, fs_handle->sigh()))
					return true;
				fs_handle->drop_notifier();
			} catch (...) { }
			return false;
		}


		/***************************
		 ** File system interface **
		 ***************************/

		static char const *name() { return "fs"; }


		/********************************
		 ** File I/O service interface **
		 ********************************/

		void write(Vfs_handle *vfs_handle, file_size len) override
		{
			Fs_vfs_handle *handle = static_cast<Fs_vfs_handle *>(vfs_handle);
			_queue_io_packet(*handle, len, Opcode::WRITE);
		}

		void read(Vfs_handle *vfs_handle, file_size len) override
		{
			Fs_vfs_handle *handle = static_cast<Fs_vfs_handle *>(vfs_handle);
			_queue_io_packet(*handle, len, Opcode::READ);
		}

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size len) override
		{
			Fs_vfs_handle const *handle = static_cast<Fs_vfs_handle *>(vfs_handle);

			try {
				_fs.truncate(handle->file_handle, len);
			}
			catch (::File_system::Invalid_handle)    { return FTRUNCATE_ERR_NO_PERM; }
			catch (::File_system::Permission_denied) { return FTRUNCATE_ERR_NO_PERM; }
			catch (::File_system::No_space)          { return FTRUNCATE_ERR_NO_SPACE; }

			return FTRUNCATE_OK;
		}

		unsigned poll(Vfs_handle *vfs_handle) override
		{
			Fs_vfs_handle *handle = static_cast<Fs_vfs_handle *>(vfs_handle);
			try { return _fs.poll(handle->file_handle); }
			catch (::File_system::Invalid_handle) { return 0; }
		}
};

#endif /* _INCLUDE__VFS__FS_FILE_SYSTEM_H_ */
