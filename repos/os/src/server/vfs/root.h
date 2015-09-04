/*
 * \brief  VFS server root
 * \author Emery Hemingway
 * \date   2015-08-17
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _VFS__ROOT_H_
#define _VFS__ROOT_H_

/* Genode includes */
#include <vfs/file_system_factory.h>
#include <os/config.h>

namespace File_system {

	Vfs::File_system *root()
	{
		static Vfs::Dir_file_system _root(config()->xml_node().sub_node("vfs"),
		                                  Vfs::global_file_system_factory());

		return &_root;
	}

}

#endif