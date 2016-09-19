/*
 * \brief  C-library back end
 * \author Christian Prochaska
 * \author Christian Helmuth
 * \date   2012-03-20
 */

/*
 * Copyright (C) 2008-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


/* Libc includes */
#include <sys/time.h>

#include "timeout.h"


extern "C" __attribute__((weak))
int _nanosleep(const struct timespec *req, struct timespec *rem)
{
	Genode::Alarm::Time duration_ms = (req->tv_sec * 1000) + (req->tv_nsec / 1000000);

	Libc::Task &task = Libc::this_task();
	Libc::Timeout to(task, duration_ms);

	while (!to.triggered())
		task.block();

	if (rem) {
		rem->tv_sec = 0;
		rem->tv_nsec = 0;
	}

	return 0;
}


extern "C" __attribute__((weak))
int nanosleep(const struct timespec *req, struct timespec *rem)
{
	return _nanosleep(req, rem);
}
