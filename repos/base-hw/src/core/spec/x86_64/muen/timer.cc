/*
 * \brief  Timer driver for core
 * \author Reto Buerki
 * \author Martin Stein
 * \date   2015-04-14
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/log.h>

/* core includes */
#include <kernel/timer.h>
#include <platform.h>
#include <board.h>
#include <sinfo_instance.h>

using namespace Genode;
using namespace Kernel;


Timer_driver::Timer_driver(unsigned) : tics_per_ms(sinfo()->get_tsc_khz())
{
	/* first sinfo instance, output status */
	sinfo()->log_status();

	struct Sinfo::Memregion_info region;
	if (!sinfo()->get_memregion_info("timed_event", &region)) {
		error("muen-timer: Unable to retrieve timed event region");
		throw Invalid_region();
	}

	event_page = (Subject_timed_event *)Platform::mmio_to_virt(region.address);
	event_page->event_nr = Board::TIMER_EVENT_KERNEL;
	log("muen-timer: Page @", Hex(region.address), ", "
	    "frequency ", tics_per_ms, " kHz, "
	    "event ", (unsigned)event_page->event_nr);

	if (sinfo()->get_memregion_info("monitor_timed_event", &region)) {
		log("muen-timer: Found guest timed event page @", Hex(region.address),
		    " -> enabling preemption");
		guest_event_page = (Subject_timed_event *)Platform::mmio_to_virt(region.address);
		guest_event_page->event_nr = Board::TIMER_EVENT_PREEMPT;
	}
}


unsigned Timer::interrupt_id() const {
	return Board::TIMER_VECTOR_KERNEL; }


void Timer::_start_one_shot(time_t const tics, unsigned)
{
	const uint64_t t = _driver.rdtsc() + tics;
	_driver.event_page->tsc_trigger = t;

	if (_driver.guest_event_page)
		_driver.guest_event_page->tsc_trigger = t;
}


time_t Timer::_tics_to_us(time_t const tics) const {
	return (tics / _driver.tics_per_ms) * 1000; }


time_t Timer::us_to_tics(time_t const us) const {
	return (us / 1000) * _driver.tics_per_ms; }


time_t Timer::_max_value() {
	return (time_t)~0; }


time_t Timer::_value(unsigned)
{
	const uint64_t now = _driver.rdtsc();
	if (_driver.event_page->tsc_trigger != Driver::TIMER_DISABLED
	    && _driver.event_page->tsc_trigger > now)
	{
		return _driver.event_page->tsc_trigger - now;
	}
	return 0;
}
