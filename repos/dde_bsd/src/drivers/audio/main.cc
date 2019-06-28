/*
 * \brief  Startup audio driver library
 * \author Josef Soentgen
 * \author Emery Hemingway
 * \date   2014-11-09
 */

/*
 * Copyright (C) 2014-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* Genode includes */
#include <audio/sink.h>
#include <audio/source.h>
#include <timer_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/session_label.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>

/* local includes */
#include <audio/audio.h>

enum { EWOULDBLOCK = 35 };


using namespace Genode;
using namespace Audio;


struct Main : Audio::Sink, Audio::Source
{
	Genode::Env        &env;
	Genode::Heap       heap { &env.ram(), &env.rm() };

	Timer::Connection  timer { env };

	// TODO: process packets on interrupt, not timeout
	Timer::Periodic_timeout<Main> packet_timeout {
		timer, *this, &Main::process_packets,
		Genode::Microseconds{(1000000 / Audio::SAMPLE_RATE) * (Audio::PERIOD-1)}
	};

	Genode::Constructible<Stereo_in>  stereo_in  { };
	Genode::Constructible<Stereo_out> stereo_out { };

	void process_packets(Genode::Duration)
	{
		if (stereo_in.constructed())
			stereo_in->progress();
		if (stereo_out.constructed())
			stereo_out->progress();
	}

	Genode::Attached_rom_dataspace config { env, "config" };

	Genode::Signal_handler<Main> config_update_dispatcher {
		env.ep(), *this, &Main::handle_config_update };

	void handle_config_update()
	{
		config.update();
		if (!config.valid()) { return; }
		Audio::update_config(env, config.xml());

		bool playback  = config.xml().attribute_value("playback", false);
		bool recording = config.xml().attribute_value("recording", false);
		if (!playback && !recording) {
			Genode::warning("driver not configured for playback or recording");
		}

		/* playback */
		if (playback) {
			if (!stereo_in.constructed()) {
				/* start the driver */
				play_silence();

				stereo_in.construct(env, *this);
			}
		} else {
			if (stereo_in.constructed())
				stereo_in.destruct();
		}

		/* recording */
		if (recording) {
			if (!stereo_out.constructed())
				stereo_out.construct(env, *this);
		} else {
			if (stereo_out.constructed())
				stereo_out.destruct();
		}
	}

	Main(Genode::Env &env) : env(env)
	{
		Audio::init_driver(env, heap, timer, config.xml());

		if (!Audio::driver_active()) {
			Genode::error("driver not active");
			env.parent().exit(~0);
			return;
		}

		config.sigh(config_update_dispatcher);
		handle_config_update();
	}

	void play_silence()
	{
		static short silence[Audio::PERIOD * 2] = { 0 };

		Audio::play(silence, sizeof(silence));
	}


	/*****************
	 ** Audio::Sink **
	 *****************/

	bool drain(float const *left, float const *right, Genode::size_t samples) override
	{
		/* convert float to S16LE */
		short data[samples * 2];

		for (unsigned i = 0; i < samples*2; i += 2) {
			data[i|0] =  left[i>>1] * 32767;
			data[i|1] = right[i>>1] * 32767;
		}

		/* send to driver */
		if (int err = Audio::play(data, sizeof(data))) {
			if (err != EWOULDBLOCK)
				Genode::error("error ", err, " during playback");
			return false;
		}

		return true;
	}


	/*******************
	 ** Audio::Source **
	 *******************/

	bool fill(float *left, float *right, Genode::size_t samples) override
	{
		static short data[2 * Audio_in::PERIOD];
		if (int err = Audio::record(data, sizeof(data))) {
				if (err && err != EWOULDBLOCK)
					Genode::warning("Error ", err, " during recording");
				return false;
		}

		float const scale = 32768.0f * 2;

		for (unsigned i = 0; i < 2*Audio_in::PERIOD; i += 2) {
			left [i/2] = data[i+0] / scale;
			right[i/2] = data[i+1] / scale;
		}

		return true;
	}
};


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Main server(env);
}

extern "C" void notify_play() { }
extern "C" void notify_record() { }
