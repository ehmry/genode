/*
 * \brief  Signal context for timer events
 * \author Josef Soentgen
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/sleep.h>
#include <base/tslab.h>
#include <timer_session/connection.h>

/* local includes */
#include <list.h>
#include <bsd.h>
#include <bsd_emul.h>


static Genode::uint64_t millisecs;


namespace Bsd {
	class Timer;
}

/**
 * Bsd::Timer
 */
class Bsd::Timer
{
	private:

		Genode::Entrypoint        &_ep;
		Genode::Timeout_scheduler &_scheduler;

		::Timer::One_shot_timeout<Bsd::Timer> _timeout {
			_scheduler, *this,
			&Bsd::Timer::_handle_timeout
		};

		/**
		 * Handle trigger_once signal
		 */
		void _handle_timeout(Genode::Duration)
		{
			Bsd::scheduler().schedule();
		}

	public:

		/**
		 * Constructor
		 */
		Timer(Genode::Entrypoint &ep,
		      Genode::Timeout_scheduler &sched)
		:
			_ep(ep), _scheduler(sched)
		{ }

		/**
		 * Update time counter
		 */
		void update_millisecs()
		{
			millisecs = _scheduler.curr_time().trunc_to_plain_ms().value;
		}

		void delay(Genode::uint64_t ms)
		{
			_timeout.schedule(Genode::Microseconds(ms*1000));
			while (_timeout.scheduled())
				_ep.wait_and_dispatch_one_io_signal();
		}
};


static Bsd::Timer *_bsd_timer;


void Bsd::timer_init(Genode::Entrypoint &ep, Genode::Timeout_scheduler &sched)
{
	/* XXX safer way preventing possible nullptr access? */
	static Bsd::Timer bsd_timer(ep, sched);
	_bsd_timer = &bsd_timer;

	/* initialize value explicitly */
	millisecs = 0;
}


void Bsd::update_time() {
	_bsd_timer->update_millisecs(); }


static Bsd::Task *_sleep_task;


/*****************
 ** sys/systm.h **
 *****************/

extern "C" int msleep(const volatile void *ident, struct mutex *mtx,
                      int priority, const char *wmesg, int timo)
{
	if (_sleep_task) {
		Genode::error("_sleep_task is not null, current task: ",
		              Bsd::scheduler().current()->name());
		Genode::sleep_forever();
	}

	_sleep_task = Bsd::scheduler().current();
	_sleep_task->block_and_schedule();

	return 0;
}

extern "C" void wakeup(const volatile void *ident)
{
	_sleep_task->unblock();
	_sleep_task = nullptr;
}


/*********************
 ** machine/param.h **
 *********************/

extern "C" void delay(int delay)
{
	_bsd_timer->delay(delay);
}
