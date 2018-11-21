/*
 * \brief  Representation of an open file
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__VFS_HANDLE_H_
#define _INCLUDE__VFS__VFS_HANDLE_H_

#include <vfs/directory_service.h>

namespace Vfs{
	class Vfs_handle;
	class File_io_service;
	class File_system;
	class Vfs_watch_handle;
	struct Handle_context;
}


/**
 * Object for informing the application of handle state.
 */
struct Vfs::Handle_context : Genode::Interface
{
	/*
	 * TODO: need we distinguish if this occurs in I/O signal dispatch?
	 */

	virtual void  read_ready() { };
	virtual void write_ready() { };

	/**
	 * Complete the current read or sync operation
	 */
	virtual void complete() { Genode::log("no application completion callback"); };

	virtual void   notify() { };
};


class Vfs::Vfs_handle
{
	private:

		Directory_service &_ds;
		File_io_service   &_fs;
		Genode::Allocator &_alloc;
		int                _status_flags;
		file_size          _seek = 0;
		Handle_context    *_context = nullptr;

		/*
		 * Noncopyable
		 */
		Vfs_handle(Vfs_handle const &);
		Vfs_handle &operator = (Vfs_handle const &);

	public:

		class Guard
		{
			private:

				/*
				 * Noncopyable
				 */
				Guard(Guard const &);
				Guard &operator = (Guard const &);

				Vfs_handle * const _handle;

			public:

				Guard(Vfs_handle *handle) : _handle(handle) { }

				~Guard()
				{
					if (_handle)
						_handle->close();
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
		 * Set application context, unset with nullptr
		 */
		virtual void context(Handle_context *context) {
			_context = context; }

		/**
		 * Get response handler, may be nullptr
		 */
		Handle_context *context() const {
			return _context; }

		inline void read_ready()
		{
			if (_context)
				_context->read_ready();
		}

		inline void write_ready()
		{
			if (_context)
				_context->write_ready();
		}

		inline void context_complete()
		{
			if (_context)
				_context->complete();
			else
				Genode::log("application context to complete"); 
		}

		/**
		 * Close handle at backing file-system.
		 *
		 * This leaves the handle pointer in an invalid and unsafe state.
		 */
		inline void close() { ds().close(this); }
};


class Vfs::Vfs_watch_handle
{
	private:

		Directory_service &_fs;
		Genode::Allocator &_alloc;
		Handle_context   *_context = nullptr;

		/*
		 * Noncopyable
		 */
		Vfs_watch_handle(Vfs_watch_handle const &);
		Vfs_watch_handle &operator = (Vfs_watch_handle const &);

	public:

		Vfs_watch_handle(Directory_service &fs,
		                 Genode::Allocator &alloc)
		:
			_fs(fs), _alloc(alloc)
		{ }

		virtual ~Vfs_watch_handle() { }

		Directory_service &fs() { return _fs; }
		Allocator &alloc() { return _alloc; }

		/**
		 * Set application context, unset with nullptr
		 */
		virtual void context(Handle_context *context) {
			_context = context; }

		/**
		 * Get application context, may be nullptr
		 */
		Handle_context *context() const {
			return _context; }

		/**
		 * Notify application through response handler
		 */
		void notify()
		{
			if (_context)
				_context->notify();
		}

		/**
		 * Close handle at backing file-system.
		 *
		 * This leaves the handle pointer in an invalid and unsafe state.
		 */
		inline void close() { fs().close(this); }
};

#endif /* _INCLUDE__VFS__VFS_HANDLE_H_ */
