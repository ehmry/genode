/*
 * \brief  C-library back end
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2010-05-19
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Libc includes */
#include <sys/time.h>

#include "task.h"
#include "libc_errno.h"

namespace Libc { time_t read_rtc(); }


extern "C" __attribute__((weak))
int clock_gettime(clockid_t clk_id, struct timespec *ts)
{
	static bool   initial_rtc_requested = false;
	static time_t initial_rtc = 0;
	static unsigned long t0_ms = 0;

	if (!ts) return Libc::Errno(EINVAL);

	Genode::Duration curr_dur = Libc::current_time();

	switch (clk_id) {

	/* IRL wall-time */
	case CLOCK_REALTIME: {
		/* try to read rtc once */
		if (!initial_rtc_requested) {
			initial_rtc_requested = true;
			initial_rtc = Libc::read_rtc();
			if (initial_rtc)
				t0_ms = curr_dur.trunc_to_plain_ms().value;
		}

		if (!initial_rtc)
			return Libc::Errno(EINVAL);

		unsigned long time = curr_dur.trunc_to_plain_ms().value - t0_ms;

		ts->tv_sec  = initial_rtc + time/1000;
		ts->tv_nsec = (time % 1000) * (1000*1000);
		break;
	}

	/* component uptime */
	case CLOCK_MONOTONIC:
	case CLOCK_UPTIME:
		ts->tv_sec = curr_dur.trunc_to_plain_ms().value/1000;
		ts->tv_nsec
			= (curr_dur.trunc_to_plain_us().value * 1000)
			% (1000*1000*1000);
		break;

	default:
		return Libc::Errno(ENOSYS);
	}

	return 0;
}


extern "C" __attribute__((weak))
int gettimeofday(struct timeval *tv, struct timezone *)
{
	if (!tv) return 0;

	struct timespec ts;

	if (int ret = clock_gettime(0, &ts))
		return ret;

	tv->tv_sec  = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / 1000;
	return 0;
}
