/*
 * \brief  Synchronous I/O wrappers
 * \author Emery Hemingway
 * \date   2016-07-08
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <vfs/file_io_service.h>
#include <vfs/vfs_handle.h>

namespace Vfs {

	Vfs::File_io_service::Write_result
	Vfs::File_io_service::write(Vfs_handle *handle, char const *src,
	                            file_size count, file_size &out_count)
	{
		struct Write_callback : Vfs::Write_callback
		{
			char const * const buffer;
			file_size    const buffer_len;
			file_size    accumulator = 0;
			Callback::Status  status = PARTIAL;

			Write_callback(char const *buf, file_size len)
			: buffer(buf), buffer_len(len) { }

			file_size write(char *dst, file_size len, Callback::Status st) override
			{
				status = st;
				if (!len) {
					return 0;
				}

				file_size out = min(len, buffer_len-accumulator);
				if (dst) {
					char const *p = buffer+accumulator;
					Genode::memcpy(dst, p, out);
				}
				accumulator += out;
				return out;
			}
		} cb { src, count };

		handle->write_callback(cb);

		write(handle, count);
		while(cb.status == Callback::PARTIAL);
			handle->fs().poll(handle);

		handle->drop_write();
		out_count = cb.accumulator;
		return (cb.status == Callback::COMPLETE) ?
			 File_io_service::WRITE_OK : File_io_service::WRITE_ERR_IO;
	}

	File_io_service::Read_result
	Vfs::File_io_service::read(Vfs_handle *handle, char *dst,
	                           file_size count, file_size &out_count)
	{
		struct Read_callback : Vfs::Read_callback
		{
			char *    const buffer;
			file_size const buffer_len;
			file_size   accumulator = 0;
			Callback::Status status = PARTIAL;

			Read_callback(char *buf, file_size len)
			: buffer(buf), buffer_len(len) { }

			file_size read(char const *src, file_size len, Callback::Status st) override
			{
				status = st;
				if (!len)
					return 0;

				file_size out = min(len, buffer_len-accumulator);
				char *p = buffer+accumulator;
				if (src)
					Genode::memcpy(p, src, len);
				else
					Genode::memset(p, 0x00, len);
				accumulator += out;
				return out;
			}
		} cb { dst, count };

		handle->read_callback(cb);

		read(handle, count);
		while(cb.status == Callback::PARTIAL);
			handle->fs().poll(handle);

		handle->drop_read();
		out_count = cb.accumulator;
		return (cb.status == Callback::COMPLETE) ?
			 File_io_service::READ_OK : File_io_service::READ_ERR_IO;
	}

}
