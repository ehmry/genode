/*
 * \brief  zero filesystem
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2012-07-31
 */

/*
 * Copyright (C) 2012-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__ZERO_FILE_SYSTEM_H_
#define _INCLUDE__VFS__ZERO_FILE_SYSTEM_H_

#include <vfs/file_system.h>

namespace Vfs { class Zero_file_system; }


struct Vfs::Zero_file_system : Single_file_system
{
	Zero_file_system(Genode::Env&,
	                 Genode::Allocator&,
	                 Genode::Xml_node config)
	:
		Single_file_system(NODE_TYPE_CHAR_DEVICE, name(),
		                   config, OPEN_MODE_RDWR)
	{ }

	static char const *name() { return "zero"; }


	/********************************
	 ** File I/O service interface **
	 ********************************/

	void write(Vfs_handle *vfs_handle, file_size len) override {
		vfs_handle->write_callback(nullptr, len, Callback::COMPLETE); }

	void read(Vfs_handle *vfs_handle, file_size len) override {
		vfs_handle->read_callback(nullptr, len, Callback::COMPLETE); }
};

#endif /* _INCLUDE__VFS__ZERO_FILE_SYSTEM_H_ */
