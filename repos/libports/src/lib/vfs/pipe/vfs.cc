/*
 * \brief  Pipe VFS plugin
 * \author Emery Hemingway
 * \date   2016-09-15
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <vfs/file_system_factory.h>
#include <vfs/file_system.h>
#include <vfs/vfs_handle.h>
#include <base/id_space.h>

namespace Vfs_pipe {
	using namespace Vfs;
	using Genode::size_t;

	enum { DEFAULT_BUFFER_SIZE = 4096 };

	struct Pipe_handle;
	typedef Genode::List<Pipe_handle> Pipe_handles;

	struct Pipe;
	typedef Genode::Id_space<Pipe> Pipe_space;

	class File_system;

	struct Pipe_handle;
	struct New_pipe_handle;
};


struct Vfs_pipe::Pipe : Pipe_space::Element
{
	Pipe_handles open_handles;

	Pipe_handle *pending_read_handle = nullptr;

	Genode::Allocator &alloc; /* allocator of this Pipe */

	Genode::size_t const buf_size;

	Genode::size_t read_offset = 0;
	Genode::size_t write_offset = 0;

	char * const buffer;

	Signal_context_capability sigh;
	bool                      notify = false;

	/**
	 * Return space available in the buffer for writing, in bytes
	 */
	size_t avail_buffer_space() const
	{
		if (read_offset < write_offset)
			return (buf_size - write_offset) + read_offset - 1;

		if (read_offset > write_offset)
			return read_offset - write_offset - 1;

		/* read_offset == write_offset */
		return buf_size - 1;
	}

	Pipe(Pipe_space &space, Genode::Allocator &alloc, Genode::size_t buf_size)
	:
		Pipe_space::Element(*this, space),
		alloc(alloc),
		buf_size(buf_size),
		buffer((char*)alloc.alloc(buf_size))
	{ }

	~Pipe()
	{
		alloc.free(buffer, buf_size);
	}

	bool any_space_avail_for_writing() const
	{
		return avail_buffer_space() > 0;;
	}

	bool data_avail_for_reading() const
	{
		return read_offset != write_offset;
	}

	size_t read(char *dst, size_t dst_len)
	{
		if (read_offset < write_offset) {

			size_t len = min(dst_len, write_offset - read_offset);
			memcpy(dst, &buffer[read_offset], len);

			read_offset += len;

			return len;
		}

		if (read_offset > write_offset) {

			size_t const upper_len = min(dst_len, buf_size - read_offset);
			memcpy(dst, &buffer[read_offset], upper_len);

			size_t const lower_len = min(dst_len - upper_len, write_offset);
			if (lower_len) {
				memcpy(dst + upper_len, &buffer[0], lower_len);
				read_offset = lower_len;
			} else {
				read_offset += upper_len;
			}

			return upper_len + lower_len;
		}

		/* read_offset == write_offset */
		return 0;
	}

	/**
	 * Write to pipe buffer
	 *
	 * \return number of written bytes (may be less than 'len')
	 */
	size_t write(char const *src, size_t len)
	{
		/* trim write request to the available buffer space */
		size_t const trimmed_len = min(len, avail_buffer_space());

		/*
		 * Remember pipe state prior writing to see if a read_ready
		 * signal should be sent
		 */
		bool const pipe_was_empty = (read_offset == write_offset);

		/* write data up to the upper boundary of the pipe buffer */
		size_t const upper_len = min(buf_size - write_offset, trimmed_len);
		memcpy(&buffer[write_offset], src, upper_len);

		write_offset += upper_len;

		/*
		 * Determine number of remaining bytes beyond the buffer boundary.
		 * The buffer wraps. So this data will end up in the lower part
		 * of the pipe buffer.
		 */
		size_t const lower_len = trimmed_len - upper_len;

		if (lower_len > 0) {

			/* pipe buffer wrap-around, write remaining data to the lower part */
			memcpy(&buffer[0], src + upper_len, lower_len);
			write_offset = lower_len;
		}

		/*
		 * Send notification
		 */
		if (pipe_was_empty && notify && sigh.valid())
			Genode::Signal_transmitter(sigh).submit();

		/* return number of written bytes */
		return trimmed_len;
	}
};


struct Vfs_pipe::Pipe_handle final : Vfs_handle, Pipe_handles::Element
{
		Pipe &pipe;
		Pipe_handle(Pipe &pipe, Vfs::File_system &fs,
		            Genode::Allocator &alloc, int flags)
		: Vfs_handle(fs, fs, alloc, flags), pipe(pipe)
	{
		pipe.open_handles.insert(this);
	}

	~Pipe_handle() { }
};

struct Vfs_pipe::New_pipe_handle final : Vfs_handle
{
	Pipe_space::Id const id;

	New_pipe_handle(Pipe_space::Id id, Vfs::File_system &fs,
	                Genode::Allocator &alloc, int flags)
	: Vfs_handle(fs, fs, alloc, flags), id(id) { }

	~New_pipe_handle() { }
};

class Vfs_pipe::File_system : public Vfs::File_system
{
	private:

		enum { MAX_NAME_LEN = 16 };

		typedef Genode::String<MAX_NAME_LEN> Pipe_name;

		Pipe_space _pipe_space;

		Genode::size_t const _buf_size;

		template <typename FUNC>
		void _apply(char const *path, FUNC const &fn)
		{
			unsigned long num = 0;
			Genode::ascii_to_unsigned(path, num, 16);
			Pipe_space::Id id { num };

			_pipe_space.apply<Pipe>(id, fn);
		}

		bool _exists(char const *path)
		{
			if (*path == '/')
				++path;

			if (strcmp(path, "new_pipe") == 0)
				return true;
			{
				bool res = false;
				_apply(path, [&] (Pipe &) { res = true; });
				return res;
			}
		}

		Io_response_handler &_io_handler;

	public:

		File_system(Genode::Allocator &alloc,
		            Genode::Xml_node node,
		            Io_response_handler &io_handler)
		:
			_buf_size(node.attribute_value("buffer", Genode::size_t(DEFAULT_BUFFER_SIZE))),
			_io_handler(io_handler)
		{ }

		char const *type() override { return "pipe"; }

		/***********************
		 ** Directory service **
		 ***********************/

		Open_result open(char const *path, unsigned mode,
		                 Vfs_handle **handle, Allocator &alloc) override
		{
			if (*path == '/')
				++path;
			/* enforce a flat file system */
			for (char const *p = path; *p; ++p)
				if (*p == '/') return OPEN_ERR_UNACCESSIBLE;

			if (strcmp(path, "new_pipe") == 0) {

				if (mode & OPEN_MODE_CREATE)
					return OPEN_ERR_EXISTS;
					/* cannot create this file */

				if (mode & OPEN_MODE_WRONLY)
					return OPEN_ERR_NO_PERM;
					/* cannot write to this file */

				/* the pipe and buffer are allocated from this caller's alloc */

				Pipe *pipe;
				try { pipe = new (alloc) Pipe(_pipe_space, alloc, _buf_size); }
				catch (Allocator::Out_of_memory) { return OPEN_ERR_NO_SPACE; }

				New_pipe_handle *meta_handle = new (alloc)
					New_pipe_handle(pipe->id(), *this, alloc, mode);

				*handle = meta_handle;

				return OPEN_OK;

			} else {

				if (mode & OPEN_MODE_CREATE) {
					return OPEN_ERR_NO_PERM;
					/* cannot manually create pipes */
				}

				if ((mode & OPEN_MODE_ACCMODE) == OPEN_MODE_RDWR) {
					return OPEN_ERR_NO_PERM;
					/* cannot insert a handle into both read and write pipe lists */
				}

				*handle = nullptr;
				_apply(path, [&] (Pipe &pipe) {
					*handle = new (alloc)
						Pipe_handle(pipe, *this, alloc, mode);
					/* constructor sets R/W handles */
				});
				return *handle == nullptr ? OPEN_ERR_UNACCESSIBLE : OPEN_OK;
			}
		}

		void close(Vfs_handle *vfs_handle) override
		{
			if (Pipe_handle *pipe_handle =
			    dynamic_cast<Pipe_handle *>(vfs_handle)) {
				Pipe &pipe = pipe_handle->pipe;

				/* dereference the pipe */
				pipe.open_handles.remove(pipe_handle);

				destroy(vfs_handle->alloc(), pipe_handle);

			} else if (New_pipe_handle *meta_handle =
			           dynamic_cast<New_pipe_handle *>(vfs_handle)) {
				destroy(vfs_handle->alloc(), meta_handle);
			}
		}

		Unlink_result unlink(char const *path) override
		{
			Unlink_result res = UNLINK_ERR_NO_ENTRY;
			_apply(path, [&] (Pipe &pipe) {

				/* may not unlink an open pipe */
				if (pipe.open_handles.first()) {
					res = UNLINK_ERR_NO_PERM;
				} else {
					destroy(pipe.alloc, &pipe);
					res = UNLINK_OK;
				}
			});
			return res;
		}

		Genode::Dataspace_capability dataspace(const char*) override {
			return Genode::Dataspace_capability(); }

		void release(const char*, Genode::Dataspace_capability) override { }

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result res = STAT_ERR_NO_ENTRY;
			out = Stat();
			_apply(path, [&] (Pipe const &pipe) {
				out.size   = _buf_size;
				out.inode  = Genode::addr_t(&pipe);
				out.device = Genode::addr_t(this);
				res = STAT_OK;
			});
			return res;
		}

		Dirent_result dirent(char const *, file_offset, Dirent&) override {
			return DIRENT_ERR_NO_PERM; }

		Readlink_result readlink(const char*, char*, file_size, file_size&) override {
			return READLINK_ERR_NO_ENTRY; }

		Rename_result rename(const char *to, const char *from) override
		{
			return (_exists(to) || _exists(from)) ?
				RENAME_ERR_NO_PERM : RENAME_ERR_NO_ENTRY;
		}

		Mkdir_result mkdir(char const *path, unsigned mode) override
		{
			return _exists(path) ?
				MKDIR_ERR_EXISTS : MKDIR_ERR_NO_PERM;
		}

		Symlink_result symlink(const char*, const char *path) override
		{
			return _exists(path) ?
				SYMLINK_ERR_EXISTS : SYMLINK_ERR_NO_PERM;
		}

		file_size num_dirent(const char*) override
		{
			return 0;
			//return _pipe_count + 1; /* plus 'new_pipe' */
		}

		bool directory(char const *path) override {
			return ((strcmp(path, "") == 0) || (strcmp(path, "/") == 0)); }

		char const *leaf_path(char const *path) override {
			return _exists(path) ? path : nullptr; }


		/**********************
		 ** File I/0 service **
		 **********************/

		Write_result write(Vfs_handle *vfs_handle,
		                   char const *src, file_size count,
		                   file_size &out_count) override
		{
			Pipe_handle *write_handle =
				dynamic_cast<Pipe_handle *>(vfs_handle);
			if (!write_handle)
				return WRITE_ERR_INVALID;

			Pipe &pipe = write_handle->pipe;
			for (;;) {
				auto n = pipe.write(src, count);
				if (n) {
					count -= n;
					src = src + n;
					out_count += n;
				}

				if (count > 0) {
					_io_handler.handle_io_response(
						pipe.pending_read_handle
							? pipe.pending_read_handle->context
							: nullptr);
				} else {
					break;
				}
			}
			return WRITE_OK;
		}

		Read_result read(Vfs_handle *vfs_handle, char *dst, file_size count,
		                 file_size &out_count) override
		{
			if (Pipe_handle *read_handle =
				dynamic_cast<Pipe_handle *>(vfs_handle))
			{
				Pipe &pipe = read_handle->pipe;
				out_count = pipe.read(dst, count);
				if (out_count == 0 && count) {
					return READ_ERR_WOULD_BLOCK;
				}
				return READ_OK;
			} else if (New_pipe_handle *meta_handle =
			           dynamic_cast<New_pipe_handle *>(vfs_handle)) {
				auto const n =
					Genode::snprintf(dst, count, "%lx\n", meta_handle->id.value);
				out_count = n-1;
				return READ_OK;
			} else {
				return READ_ERR_INVALID;
			}
		}

		bool queue_read(Vfs_handle *vfs_handle, char *dst, file_size count,
		                Read_result &out_result, file_size &out_count) override
		{
			if (Pipe_handle *handle =
				dynamic_cast<Pipe_handle *>(vfs_handle))
			{
				Pipe &pipe = handle->pipe;
				out_count = pipe.read(dst, count);
				if (out_count) {
					out_result = READ_OK;
					return true;
				} else {
					if (pipe.pending_read_handle) {
						return false;
					}
					pipe.pending_read_handle = handle;
					out_result = READ_QUEUED;
					return true;
				}
			} else if (New_pipe_handle *meta_handle =
			           dynamic_cast<New_pipe_handle *>(vfs_handle)) {
				out_result = read(meta_handle, dst, count, out_count);
				return true;
			} else {
				out_result = READ_ERR_INVALID;
				return true;
			}
		}

		Read_result complete_read(Vfs_handle *vfs_handle,
		                          char *dst, file_size count,
		                          file_size &out_count) override
		{
			Pipe_handle *handle = dynamic_cast<Pipe_handle *>(vfs_handle);
			if (!handle)
				return READ_ERR_INVALID;
			Pipe &pipe = handle->pipe;
			if (handle != pipe.pending_read_handle)
				return READ_ERR_INVALID;

			out_count = pipe.read(dst, count);
			if (out_count) {
				pipe.pending_read_handle = nullptr;
				return READ_OK;
			} else {
				return READ_QUEUED;
			}
		}

		Ftruncate_result ftruncate(Vfs_handle*, file_size len) override {
			return len ? FTRUNCATE_ERR_NO_PERM : FTRUNCATE_OK; }

		bool check_unblock(Vfs_handle *vfs_handle, bool rd, bool wr, bool ex) override
		{
			if (Pipe_handle *handle =
				dynamic_cast<Pipe_handle *>(vfs_handle))
			{
				Pipe &pipe = handle->pipe;
				return
					(rd ? pipe.data_avail_for_reading()  : true) &&
					(wr ? pipe.any_space_avail_for_writing() : true);
			}
			return false;
		}

		bool read_ready(Vfs_handle *vfs_handle) override
		{
			if (Pipe_handle *read_handle =
				dynamic_cast<Pipe_handle *>(vfs_handle))
				return read_handle->pipe.data_avail_for_reading();
			return false;
		}

		bool notify_read_ready(Vfs_handle *vfs_handle) override
		{
			if (Pipe_handle *read_handle =
				dynamic_cast<Pipe_handle *>(vfs_handle))
			{
				read_handle->pipe.notify = true;
				return true;
			}
			return false;
		}

		void register_read_ready_sigh(Vfs_handle *vfs_handle,
		                              Signal_context_capability sigh) override
		{
			if (Pipe_handle *handle =
				dynamic_cast<Pipe_handle *>(vfs_handle))
			{
				handle->pipe.sigh = sigh;
			}
		}
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Pipe_factory final : Vfs::File_system_factory
	{
		Vfs::File_system *create(Genode::Env &env,
		                         Genode::Allocator &alloc,
		                         Genode::Xml_node node,
		                         Vfs::Io_response_handler &io_handler) override
		{
			return new (alloc)
				Vfs_pipe::File_system(alloc, node, io_handler);
		}
	};

	static Pipe_factory factory;
	return &factory;
}
