/*
 * \brief  System time support for lwIP
 * \author Emery Hemingway
 * \date   2016-12-01
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <timer_session/connection.h>
#include <util/reconstructible.h>

namespace Lwip {

/* LwIP includes */
#include <lwip/timeouts.h>
#include <lwip/init.h>
#include <lwip/sys.h>

	/**
	 * XXX: can this be replaced with an alarm?
	 */
	struct Sys_timer : Timer::Connection, Genode::Signal_handler<Sys_timer>
	{
		enum { WAKEUP_MS = 1 << 20 };

		void timeout()
		{
			Lwip::sys_check_timeouts();
			trigger_once(WAKEUP_MS);
		}

		Sys_timer(Genode::Env &env)
		:
			Timer::Connection(env, "lwIP"),
			Genode::Signal_handler<Sys_timer>(env.ep(), *this, &Sys_timer::timeout)
		{
			sigh(*this);
			trigger_once(WAKEUP_MS);
		}
	};

	static Genode::Constructible<Sys_timer> sys_timer;
}


void lwip_init(Genode::Env &env)
{
	Lwip::sys_timer.construct(env);
	Lwip::lwip_init();
}


extern "C" {

	Lwip::u32_t sys_now(void) {
		return Lwip::sys_timer->elapsed_ms(); }

	void genode_memcpy(void * dst, const void *src, unsigned long size) {
		Genode::memcpy(dst, src, size); }

}
