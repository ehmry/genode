/*
 * \brief  Dummy audio driver
 * \author Emery Hemingway
 * \date   2018-06-05
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <audio/sink.h>
#include <timer_session/connection.h>
#include <base/component.h>

namespace Dummy {
	using namespace Genode;
	using namespace Audio_in;
	struct Driver;
};


struct Dummy::Driver : Audio::Sink
{
	Genode::Env &env;

	Stereo_in stereo_in { env, *this };

	Timer::Connection timer { env, "sync" };

	enum { PROGRESS_RATE_US =
		((1000*1000) / SAMPLE_RATE) * (PERIOD-1) };

	Timer::Periodic_timeout<Driver> timeout {
		timer, *this, &Driver::sync,
		Microseconds(PROGRESS_RATE_US) };

	Signal_context_capability progress_sigh { };

	void sync(Duration)
	{
		stereo_in.progress();
	}

	Driver(Genode::Env &e) : env(e) { };


	/*****************
	 ** Audio::Sink **
	 *****************/

	bool drain(float const *, float const *, Genode::size_t) override
	{
		return true;
	}
};


void Component::construct(Genode::Env &env)
{
	static Dummy::Driver inst(env);
}
