/*
 * \brief  Startup audio driver library
 * \author Josef Soentgen
 * \date   2014-11-09
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* Genode includes */
#include <audio_in_session/connection.h>
#include <audio_out_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/session_label.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>

/* local includes */
#include <audio/audio.h>


using namespace Genode;
using namespace Audio;


/**************
 ** Playback **
 **************/

namespace Playback {
	using namespace Audio_in;
	class Out;
}

class Playback::Out
{
	private:

		Audio_in::Connection _left;
		Audio_in::Connection _right;

		Genode::Signal_handler<Playback::Out> _notify_handler;

		void _play_silence()
		{
			static short silence[Audio_in::PERIOD * Playback::MAX_CHANNELS] = { 0 };

			int err = Audio::play(silence, sizeof(silence));
			if (err && err != 35) {
				Genode::warning("Error ", err, " during silence playback");
			}
		}

		/**
		 * DMA block played
		 */
		void _play_packet()
		{
			Stream *left  =  _left.stream();
			Stream *right = _right.stream();

			auto const pos = left->pos();

			Packet *p_left  =  left->get(pos);
			Packet *p_right = right->get(pos);

			if (p_left->valid() && p_right->valid()) {
				/* convert float to S16LE */
				static short data[Audio_in::PERIOD * Playback::MAX_CHANNELS];

				for (int i = 0; i < Audio_in::PERIOD * Playback::MAX_CHANNELS; i += 2) {
					data[i] = p_left->content()[i / 2] * 32767;
					data[i + 1] = p_right->content()[i / 2] * 32767;
				}

				/* send to driver */
				if (int err = Audio::play(data, sizeof(data))) {
					Genode::warning("Error ", err, " during playback");
				}

			} else {
				_play_silence();
			}

			 left->increment_position();
			right->increment_position();
		}

	public:

		Out(Genode::Env &env)
		:
			_left(env, "left",   false),
			_right(env, "right", false),
			_notify_handler(env.ep(), *this, &Playback::Out::_play_packet)
		{
			_left.start();
			_right.start();
			/* play a silence packet to get the driver running */
			_play_silence();
		}

		Signal_context_capability sigh() { return _notify_handler; }
};


/***************
 ** Recording **
 ***************/

namespace Recording {
	using namespace Audio_out;
	class In;
}

class Recording::In
{
	private:

		Audio_out::Connection          _audio_out;
		Signal_handler<Recording::In>  _notify_handler;

		void _record_packet()
		{
			static short data[2 * Audio_out::PERIOD];
			if (int err = Audio::record(data, sizeof(data))) {
					if (err && err != 35) {
						Genode::warning("Error ", err, " during recording");
					}
					return;
			}

			Stream *stream = _audio_out.stream();

			// XXX: next or alloc?
			Packet *p = stream->next();

			float const scale = 32768.0f * 2;

			float * const content = p->content();
			for (int i = 0; i < 2*(int)Audio_out::PERIOD; i += 2) {
				float sample = data[i] + data[i+1];
				content[i/2] = sample / scale;
			}

			_audio_out.submit(p);
		}

	public:

		In(Genode::Env &env)
		:
			_audio_out(env, "center", false, false),
			_notify_handler(env.ep(), *this, &Recording::In::_record_packet)
		{
			_audio_out.start();
			_record_packet();
		}

		Signal_context_capability sigh() { return _notify_handler; }
};


/**********
 ** Main **
 **********/

struct Main
{
	Genode::Env        &env;
	Genode::Heap       heap { &env.ram(), &env.rm() };

	Genode::Attached_rom_dataspace config { env, "config" };

	Genode::Signal_handler<Main> config_update_handler {
		env.ep(), *this, &Main::handle_config_update };

	void handle_config_update()
	{
		config.update();
		if (!config.is_valid()) { return; }
		Audio::update_config(env, config.xml());
	}

	Main(Genode::Env &env) : env(env)
	{
		Audio::init_driver(env, heap, config.xml());

		if (!Audio::driver_active()) {
			return;
		}

		/* playback */
		if (config.xml().attribute_value("playback", true)) {
			static Playback::Out out(env);
			Audio::play_sigh(out.sigh());

			Genode::log("--- BSD Audio driver enable playback ---");
		}

		/* recording */
		if (config.xml().attribute_value("recording", false)) {
			static Recording::In in(env);
			Audio::record_sigh(in.sigh());

			Genode::log("--- BSD Audio driver enable recording ---");
		}

		config.sigh(config_update_handler);
	}
};


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Main server(env);
}
