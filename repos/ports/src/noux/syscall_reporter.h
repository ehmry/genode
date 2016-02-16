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
#include <os/reporter.h>
#include <trace/timestamp.h>


namespace Noux { struct Syscall_reporter; }

struct Noux::Syscall_reporter : Genode::Reporter
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
	: Genode::Reporter("syscalls")
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

	void submit()
	{
		Genode::Reporter::enabled(true);

		float everything = 0;
		for (int i = 0; i < Session::NUM_SYSCALLS; ++i)
			everything += collection[i].total;

		Genode::Reporter::Xml_generator xml((Genode::Reporter &)*this, [&] ()
		{
			for (int i = 0; i < Session::NUM_SYSCALLS; ++i) {
				Call_data &data = collection[i];
				if (data.count) xml.node(Noux::Session::syscall_name(i), [&] () {
					xml.attribute("relative", int(100.0*data.total/everything));
					xml.attribute("calls", data.count);
					xml.attribute("min", data.min);
					xml.attribute("avg", data.total/data.count);
					xml.attribute("max", data.max);
					xml.attribute("total", data.total);
				});
			}
		});
	}

};


#endif /* _NOUX__SYSCALL_REPORTER_H_ */
