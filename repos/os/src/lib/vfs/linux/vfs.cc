/*
 * \brief  Linux VFS plugin
 * \author Emery Hemingway
 * \date   2015-09-05
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <vfs/file_system_factory.h>

/* local includes */
#include <vfs_linux.h>


struct Linux_factory : Vfs::File_system_factory
{
	Vfs::File_system *create(Genode::Xml_node node) override
	{
		PDBG("");
		return new (Genode::env()->heap()) Linux_file_system(node);
	}
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	static Linux_factory factory;
	return &factory;
}
