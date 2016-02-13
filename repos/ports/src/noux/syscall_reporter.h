/*
 * \brief  Record and report syscall statistics
 * \author Emery Hemingway
 * \date   2016-02-13
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__SYSCALL_REPORTER_H_
#define _NOUX__SYSCALL_REPORTER_H_

/* Noux includes */
#include <noux_session/noux_session.h>

/* Genode includes*/
#include <trace/timestamp.h>


namespace Noux { struct Syscall_reporter; }

struct Noux::Syscall_reporter
{
	struct Call_data
	{
		unsigned long total = 0;
		unsigned count = 0;
		unsigned min = ~0;
		unsigned max =  0;
	};

	Call_data collection[Session::NUM_SYSCALLS];

	Call_data *data;
	Genode::Trace::Timestamp onset;

	Syscall_reporter()
	{
		for (auto i = 0; i < Session::NUM_SYSCALLS; ++i)
			collection[i] = Call_data();
	}

	void start(Session::Syscall sc)
	{
		if (sc < 0 || sc >= int(Session::NUM_SYSCALLS))
			return;

		data = &collection[sc];
		onset = Genode::Trace::timestamp();
	}

	void end()
	{
		if (!data) return;
		Genode::Trace::Timestamp elapsed =
			Genode::Trace::timestamp() - onset;

		data->total += elapsed;
		++data->count;
		if (elapsed < data->min)
			data->min = elapsed;
		if (elapsed > data->max)
			data->max = elapsed;
		data = nullptr;
	}

	void print()
	{
		float everything = 0;
		for (int i = 0; i < Session::NUM_SYSCALLS; ++i)
			everything += collection[i].total;

		for (int i = 0; i < Session::NUM_SYSCALLS; ++i) {
			Call_data &data = collection[i];
			if (!data.count) continue;

			Genode::printf("%d%% %s\tcalls=%u min=%u avg=%lu max=%u\n",
				int(100.0*(data.total/everything)), Noux::Session::syscall_name(i),
				data.count, data.min, data.total/data.count, data.max);
		}
	}

};


#endif /* _NOUX__SYSCALL_REPORTER_H_ */
