/*
 * \brief  Audio_out-out driver for Linux
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Emery Hemingway
 * \date   2010-05-11
 */

/*
 * Copyright (C) 2010-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <audio/sink.h>
#include <timer_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>

/* local includes */
#include <alsa.h>

namespace Audio
{
	using namespace Genode;
	struct Driver;
};


struct Audio::Driver : Audio::Sink
{
	Stereo_in stereo_in;

	Timer::Connection timer;

	Timer::Periodic_timeout<Driver> packet_timeout {
		timer, *this, &Driver::process,
		Microseconds{(1000000 / Audio::SAMPLE_RATE) * (Audio::PERIOD-1)}
	};

	Driver(Genode::Env &env) : stereo_in(env, *this), timer(env)
	{
		audio_drv_start();
	}

	void process(Duration) {
		stereo_in.progress(); }


	/*****************
	 ** Audio::Sink **
	 *****************/

	bool drain(float const *left, float const *right, size_t samples) override
	{
		/* convert float to S16LE */
		short data[2 * samples];

		for (unsigned i = 0; i < 2 * samples; i += 2) {
			data[i|0] =  left[i>>1] * 32767;
			data[i|1] = right[i>>1] * 32767;
		}

		/* blocking-write packet to ALSA */
		if (audio_drv_play(data, samples)) {
			/* try to restart the driver silently */
			audio_drv_stop();
			audio_drv_start();
			return false;
		}

		return true;
	}
};


/***************
 ** Component **
 ***************/

void Component::construct(Genode::Env &env)
{
	Genode::Attached_rom_dataspace config { env, "config" };

	typedef Genode::String<32> String;

	auto dev = config.xml().attribute_value("alsa_device", String("hw"));

	/* init ALSA */
	if (int err = audio_drv_init(dev.string())) {
		if (err == -1) {
			Genode::error("could not open ALSA device ", dev);
		} else {
			Genode::error("could not initialize driver error ", err);
		}
		env.parent().exit(err);
		return;
	}

	static Audio::Driver inst(env);
}
