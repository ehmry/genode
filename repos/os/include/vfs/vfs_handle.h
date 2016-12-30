/*
 * \brief  Representation of an open file
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__VFS_HANDLE_H_
#define _INCLUDE__VFS__VFS_HANDLE_H_

#include <vfs/file_io_service.h>
#include <vfs/directory_service.h>

namespace Vfs {
	struct Callback;
	struct Write_callback;
	struct Read_callback;
	struct Notify_callback;
	class Vfs_handle;
}


struct Vfs::Callback
{
	/**
	 * Operation status passed to I/O callbacks
	 *
	 * When an operation returns a QUEUED result, expect
	 * callbacks until a COMPLETE or ERROR status is
	 * passed to a callback. Otherwise callbacks must
	 * expect to only receive PARTIAL.
	 */
	enum Status {
		COMPLETE,       /* request completed              */
		PARTIAL,        /* request partially completed    */
		ERR_IO,         /* internal error                 */
		ERR_INVALID,    /* handle or operation is invalid */
		ERR_CLOSED      /* connection closed              */
	};
};


struct Vfs::Write_callback : Callback
{
	/**
	 * \param status  Status of request.
	 * \param dst     Data to be written. a null-pointer
	 *                indicates a discard.
	 * \param len     Length of buffer or discard count.
	 * \return        Length of buffer processed by callback;
	 */
	virtual file_size write(char *dst, file_size len,
	                        Callback::Status status) = 0;
};


struct Vfs::Read_callback : Callback
{
	/**
	 * \param status  Status of request.
	 * \param src     Data to be read. A null-pointer indicates
	 *                that the backend operation produced 'len'
	 *                null bytes.
	 * \param len     Length of buffer or null byte count.
	 * \return        Length of buffer processed by callback;
	 */
	virtual file_size read(char const *src, file_size len,
	                       Callback::Status status) = 0;
};


struct Vfs::Notify_callback
{
	virtual void notify() = 0;
};


class Vfs::Vfs_handle
{
	private:

		Directory_service &_ds;
		File_io_service   &_fs;
		Genode::Allocator &_alloc;
		Write_callback    *_w_cb = nullptr;
		Read_callback     *_r_cb = nullptr;
		Notify_callback   *_n_cb = nullptr;
		int                _status_flags;
		file_size          _seek = 0;

	public:

		struct Guard
		{
			Vfs_handle *handle;

			Guard(Vfs_handle *handle) : handle(handle) { }

			~Guard()
			{
				if (handle)
					handle->_ds.close(handle);
			}
		};

		enum { STATUS_RDONLY = 0, STATUS_WRONLY = 1, STATUS_RDWR = 2 };

		Vfs_handle(Directory_service &ds,
		           File_io_service   &fs,
		           Genode::Allocator &alloc,
		           int                status_flags)
		:
			_ds(ds), _fs(fs),
			_alloc(alloc),
			_status_flags(status_flags)
		{ }

		virtual ~Vfs_handle() { }

		Directory_service &ds() { return _ds; }
		File_io_service   &fs() { return _fs; }
		Allocator      &alloc() { return _alloc; }

		int status_flags() const { return _status_flags; }

		void status_flags(int flags) { _status_flags = flags; }

		/**
		 * Return seek offset in bytes
		 */
		file_size seek() const { return _seek; }

		/**
		 * Set seek offset in bytes
		 */
		void seek(file_offset seek) { _seek = seek; }

		/**
		 * Advance seek offset by 'incr' bytes
		 */
		void advance_seek(file_size incr) { _seek += incr; }

		/**
		 * Add or replace write callback
		 */
		void write_callback(Write_callback &cb) { _w_cb = &cb; }

		/**
		 * Remove write callback
		 */
		void drop_write() { _w_cb = nullptr; };

		/**
		 * Execute write callback
		 */
		file_size write_callback(char *dst, file_size dst_len,
		                         Callback::Status status) const
		{
			return _w_cb ?
				_w_cb->write(dst, dst_len, status) : 0;
		}

		/**
		 * Idiom for a status only callback
		 */
		inline void write_status(Callback::Status status) {
			write_callback(nullptr, 0, status); }

		/**
		 * Add or replace read callback
		 */
		void read_callback(Read_callback &cb) { _r_cb = &cb; }

		/**
		 * Remove read callback
		 */
		void drop_read() { _r_cb = nullptr; };

		/**
		 * Execute read callback
		 */
		file_size read_callback(char const *src, file_size src_len,
		                        Callback::Status status) const
		{
			return _r_cb ?
				_r_cb->read(src, src_len, status) : 0;
		}

		/**
		 * Idiom for a status only callback
		 */
		inline void read_status(Callback::Status status) {
			read_callback(nullptr, 0, status); }

		/**
		 * Add or replace notify callback
		 *
		 * Handles must be registered with 'notify' at the
		 * directory service if callbacks are to be issued.
		 */
		void notify_callback(Notify_callback &cb) { _n_cb = &cb; }

		/**
		 * Remove notify callback
		 */
		void drop_notify() { _n_cb = nullptr; };

		/**
		 * Execute notify callback
		 */
		void notify_callback()
		{
			if (_n_cb)
				_n_cb->notify();
		}
};

#endif /* _INCLUDE__VFS__VFS_HANDLE_H_ */
