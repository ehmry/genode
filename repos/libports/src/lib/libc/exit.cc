/*
 * \brief  C-library back end
 * \author Norman Feske
 * \date   2008-11-11
 */

/*
 * Copyright (C) 2008-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/sleep.h>

/* Libc includes */
#include <libc-plugin/fd_alloc.h>
#include <unistd.h>

extern void genode_exit(int status) __attribute__((noreturn));

extern "C" void _exit(int status)
{
	using namespace Libc;

	/* flush and close all registered descriptors */
	auto const close_fn = [] (Libc::File_descriptor &fd) {
		::fsync(fd.libc_fd);
		if (fd.plugin) {
			::close(fd.libc_fd);
		} else {
			/*
			 * stdio descriptors have no plugin,
			 * just free and continue
			 */
			file_descriptor_allocator()->free(&fd);
		}
	};

	File_descriptor_allocator &fd_alloc =
		*file_descriptor_allocator();
	while (fd_alloc.apply_any(close_fn)) { }

	/* calls `atexit` procedures */
	genode_exit(status);
}


extern "C" {

	/* as provided by the original stdlib/exit.c */
	int __isthreaded = 0;

	void (*__cleanup)(void);

	void exit(int status)
	{
		if (__cleanup)
			(*__cleanup)();

		_exit(status);
	}
}
