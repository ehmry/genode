/*
 * \brief  Inline filesystem
 * \author Norman Feske
 * \date   2014-04-14
 *
 * This file system allows the content of a file being specified as the content
 * of its config node.
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__INLINE_FILE_SYSTEM_H_
#define _INCLUDE__VFS__INLINE_FILE_SYSTEM_H_

#include <vfs/single_file_system.h>

namespace Vfs { class Inline_file_system; }


class Vfs::Inline_file_system : public Single_file_system
{
	private:

		char const * const _base;
		file_size    const _size;

	public:

		Inline_file_system(Genode::Env&,
		                   Genode::Allocator&,
		                   Genode::Xml_node config)
		:
			Single_file_system(NODE_TYPE_FILE, name(),
			                   config, OPEN_MODE_RDONLY),
			_base(config.content_base()),
			_size(config.content_size())
		{ }

		static char const *name() { return "inline"; }


		/********************************
		 ** Directory service interface **
		 ********************************/

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = _size;
			return result;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		void read(Vfs_handle *vfs_handle, file_size count) override
		{
			/* file read limit is the size of the dataspace */
			file_size const max_size = _size;

			/* current read offset */
			file_size const read_offset = vfs_handle->seek();

			/* maximum read offset, clamped to dataspace size */
			file_size const end_offset = min(count + read_offset, max_size);

			/* source address within the dataspace */
			char const *src = _base + read_offset;

			/* check if end of file is reached */
			if (read_offset >= end_offset)
				return vfs_handle->read_status(Callback::COMPLETE);

			/* pass ROM dataspace to callback */
			count = end_offset - read_offset;
			vfs_handle->read_callback(src, count, Callback::COMPLETE);
		}
};

#endif /* _INCLUDE__VFS__INLINE_FILE_SYSTEM_H_ */
