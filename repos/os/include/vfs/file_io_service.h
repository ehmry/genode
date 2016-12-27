/*
 * \brief  Interface for operations provided by file I/O service
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__FILE_IO_SERVICE_H_
#define _INCLUDE__VFS__FILE_IO_SERVICE_H_

#include <vfs/types.h>
#include <base/debug.h>

namespace Vfs {
	class Vfs_handle;
	struct File_io_service;

	enum Poll : unsigned {
		 READ_READY = 0x01U,
		WRITE_READY = 0x02U
	};
}


struct Vfs::File_io_service
{
	enum General_error { ERR_FD_INVALID, NUM_GENERAL_ERRORS };


	/***********
	 ** Write **
	 ***********/

	enum Write_result { WRITE_ERR_AGAIN,     WRITE_ERR_WOULD_BLOCK,
	                    WRITE_ERR_INVALID,   WRITE_ERR_IO,
	                    WRITE_ERR_INTERRUPT,
	                    WRITE_QUEUED, WRITE_OK };

	/**
	 * Submit a write request
	 *
	 * \param handle  handle to execute callbacks from
	 * \param len     number of bytes to write or queue
	 */
	virtual void write(Vfs_handle *handle, file_size len) = 0;

	/**
	 * Synchronous write
	 */
	Write_result write(Vfs_handle *handle, char const *buf,
	                   file_size buf_size, file_size &out_count);


	/**********
	 ** Read **
	 **********/

	enum Read_result { READ_ERR_AGAIN,     READ_ERR_WOULD_BLOCK,
	                   READ_ERR_INVALID,   READ_ERR_IO,
	                   READ_ERR_INTERRUPT,
	                   READ_QUEUED, READ_OK };

	/**
	 * Submit a read request
	 *
	 * \param handle  handle to execute callbacks from
	 * \param len     number of bytes to read or queue
	 */
	virtual void read(Vfs_handle *handle, file_size len) = 0;

	/**
	 * Synchronous read
	 */
	Read_result read(Vfs_handle *handle, char *dst,
	                 file_size count, file_size &out_count);


	/***************
	 ** Ftruncate **
	 ***************/

	enum Ftruncate_result { FTRUNCATE_ERR_NO_PERM = NUM_GENERAL_ERRORS,
	                        FTRUNCATE_ERR_INTERRUPT, FTRUNCATE_ERR_NO_SPACE,
	                        FTRUNCATE_OK };

	virtual Ftruncate_result ftruncate(Vfs_handle *handle, file_size len) = 0;


	/***********
	 ** Ioctl **
	 ***********/

	enum Ioctl_result { IOCTL_ERR_INVALID, IOCTL_ERR_NOTTY, IOCTL_OK };

	enum Ioctl_opcode { IOCTL_OP_UNDEFINED, IOCTL_OP_TIOCGWINSZ,
	                    IOCTL_OP_TIOCSETAF, IOCTL_OP_TIOCSETAW,
	                    IOCTL_OP_FIONBIO,   IOCTL_OP_DIOCGMEDIASIZE };

	enum Ioctl_value { IOCTL_VAL_NULL, IOCTL_VAL_ECHO, IOCTL_VAL_ECHONL };

	typedef unsigned long Ioctl_arg;

	struct Ioctl_out
	{
		union
		{
			/* if request was 'IOCTL_OP_TIOCGWINSZ' */
			struct {
				int rows;
				int columns;
			} tiocgwinsz;

			/* if request was 'IOCTL_OP_DIOCGMEDIASIZE' */
			struct {
				/* disk size rounded up to sector size in bytes*/
				int size;

			} diocgmediasize;
		};
	};

	virtual Ioctl_result ioctl(Vfs_handle *handle, Ioctl_opcode, Ioctl_arg,
	                           Ioctl_out &out)
	{
		/*
		 * This method is only needed in file systems which actually implement a
		 * device and is therefore false by default.
		 */
		return IOCTL_ERR_INVALID;
	}

	/**
	 * Return true if an unblocking condition of the file is satisfied
	 *
	 * Deprecated, I/O ops should return zero if blocked.
	 *
	 * \param rd  if true, check for data available for reading
	 * \param wr  if true, check for readiness for writing
	 * \param ex  if true, check for exceptions
	 */
	virtual bool check_unblock(Vfs_handle *handle,
	                           bool rd, bool wr, bool ex)
	{ return true; }

	/**
	 * Deprecated, use a notify callback
	 */
	virtual void register_read_ready_sigh(Vfs_handle *handle,
	                                      Signal_context_capability sigh)
	{ }

	/**
	 * Poll handle state
	 */
	virtual unsigned poll(Vfs_handle *handle) = 0;
};

#endif /* _INCLUDE__VFS__FILE_IO_SERVICE_H_ */
