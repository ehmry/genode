/*
 * \brief  Timer driver for core
 * \author Sebastian Sumpf
 * \date   2015-08-22
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Core includes */
#include <kernel/timer.h>
#include <machine_call.h>

using namespace Genode;
using namespace Kernel;


Timer_driver::Timer_driver(unsigned)
{
	/* enable timer interrupt */
	enum { STIE = 0x20 };
	asm volatile ("csrs sie, %0" : : "r"(STIE));
}


void Timer::_start_one_shot(time_t const tics, unsigned const)
{
	_driver.timeout = _driver.stime() + tics;
	Machine::set_sys_timer(_driver.timeout);
}


time_t Timer::_tics_to_us(time_t const tics) const {
	return (tics / Driver::TICS_PER_MS) * 1000; }


time_t Timer::us_to_tics(time_t const us) const {
	return (us / 1000) * Driver::TICS_PER_MS; }


time_t Timer::_max_value() {
	return (addr_t)~0; }


time_t Timer::_value(unsigned const)
{
	addr_t time = _driver.stime();
	return time < _driver.timeout ? _driver.timeout - time : 0;
}


unsigned Timer::interrupt_id() const {
	return 1; }
