/*
 * \brief  Dummy audio driver
 * \author Emery Hemingway
 * \date   2018-06-05
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <audio_in_session/connection.h>
#include <timer_session/connection.h>
#include <base/component.h>
#include <base/log.h>
#include <util/reconstructible.h>

namespace Dummy {
	using namespace Genode;
	using namespace Audio_in;
	struct Driver;
};


struct Dummy::Driver
{
	Genode::Env &env;

	Audio_in::Connection  left { env, "left" };
	Audio_in::Connection right { env, "right" };

	Timer::Connection timer { env, "sync" };

	enum { PROGRESS_RATE_US =
		((1000*1000) / SAMPLE_RATE) * PERIOD };

	Constructible<Timer::Periodic_timeout<Driver>> timeout { };

	Signal_context_capability underrun_sigh { };

	void sync(Duration)
	{
		unsigned pos = left.stream().play_pos();
		 left.stream().play(pos, [&] (Audio::Packet &) { });
		right.stream().play(pos, [&] (Audio::Packet &) { });
		unsigned q = left.stream().queued();
		if (!q || q == Audio::UNDERRUN_THRESHOLD) {
			log("submit underrun signal, ", q, " packets queued");
			Signal_transmitter(underrun_sigh).submit();
			if (!q) {
				log("stop timeout");
				timeout.destruct();
			}
		}
	}

	Signal_handler<Driver> start_handler {
		env.ep(), *this, &Driver::start };

	Signal_handler<Driver> reset_handler {
		env.ep(), *this, &Driver::reset };

	void start()
	{
		if (!timeout.constructed())
			timeout.construct(
				timer, *this, &Driver::sync,
				Microseconds(PROGRESS_RATE_US));
		left.stream().seek();
		log("start playback at position ", left.stream().play_pos());
	}

	void reset()
	{
		left.start_sigh(start_handler);
		left.reset_sigh(reset_handler);
		underrun_sigh = left.underrun_sigh();
		if (underrun_sigh.valid()) {
			log("got a valid underrun handler");
		} else {
			error("underrun handler not valid");			
		}
	}

	Driver(Genode::Env &e) : env(e)
	{
		reset();
	};
};


void Component::construct(Genode::Env &env)
{
	static Dummy::Driver inst(env);
}
