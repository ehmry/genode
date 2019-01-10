/*
 * \brief  Utility for loading a file
 * \author Norman Feske
 * \date   2014-08-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* gems includes */
#include <gems/file.h>

/* libc includes */
#include <libc/component.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>


static Genode::size_t file_size(char const *name)
{
	struct stat s;
	s.st_size = 0;
	Libc::with_libc([&] () { stat(name, &s); });
	if (s.st_size < 1)
		throw File::Reading_failed();

	return s.st_size;
}


File::File(char const *name, Genode::Allocator &alloc)
:
	_alloc(alloc),
	_file_size(file_size(name)),
	_data(alloc.alloc(_file_size))
{
	/* do not throw in with_libc, throw at the tail */
	bool success = false;

	Libc::with_libc([&] () {
		int const fd = open(name, O_RDONLY);
		if (fd < 0) return;

		Genode::size_t  remain = _file_size;
		char           *data   = (char *)_data;
		do {
			int ret;
			if ((ret = read(fd, data, remain)) < 0) {
				Genode::error("reading from file \"", name, "\" failed (error ", errno, ")");
				return;
			}
			remain -= ret;
			data   += ret;
		} while (remain > 0);

		close(fd);
		success = true;
	});

	if (!success)
		Reading_failed();
}


File::~File()
{
	_alloc.free(_data, _file_size);
}
