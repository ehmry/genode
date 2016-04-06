/*
 * \brief  VFS short read test
 * \author Emery Hemingway
 * \date   2016-04-04
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <vfs/file_system_factory.h>
#include <vfs/dir_file_system.h>
#include <os/config.h>
#include <timer_session/connection.h>
#include <trace/timestamp.h>
#include <base/process.h>


int main()
{
	using namespace Genode;
	using namespace Vfs;

	/* look for dynamic linker */
	try {
		static Genode::Rom_connection rom("ld.lib.so");
		Genode::Process::dynamic_linker(rom.dataspace());
	} catch (...) { }

	static Vfs::Dir_file_system vfs =
		{ config()->xml_node().sub_node("vfs"),
		  Vfs::global_file_system_factory()
		};

	Vfs_handle *handle;
	static unsigned mode =
		Vfs::Directory_service::OPEN_MODE_CREATE |
		Vfs::Directory_service::OPEN_MODE_RDWR;
	if (vfs.open("/test", mode, &handle)
	    != Directory_service::Open_result::OPEN_OK)
		return ~1;

	enum { TEST_SIZE = (1U << 20) };

	handle->fs().ftruncate(handle, TEST_SIZE);

	Timer::Connection timer;

	char buf[256];
	file_size count = 0;
	int read_count = 0;
	int x = 65537;
	file_size out = 0;
	while (count < TEST_SIZE) {
		x = (x * 17) % 257;
		handle->fs().read(handle, buf, x, out);
		handle->advance_seek(x);
		count += out;
		++read_count;
	}
	PLOG("%d short reads in %lums", read_count, timer.elapsed_ms());
	return 0;
}
