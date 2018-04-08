/*
 * \brief  Audio_out-out driver for Linux
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2010-05-11
 *
 * FIXME session and driver shutdown not implemented (audio_drv_stop)
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <audio_in_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/log.h>
#include <util/misc_math.h>

/* local includes */
#include <alsa.h>


using namespace Genode;

enum Channel_number { LEFT, RIGHT, MAX_CHANNELS, INVALID = MAX_CHANNELS };

namespace Playback {
	using namespace Audio_in;
	class Out;
	struct Main;
};

namespace Alsa_driver { struct Main; }

/*
 * Root component, handling new session requests.
 */
class Playback::Out
{
	private:

		Genode::Env          &_env;
		Audio_in::Connection  _left  { _env, "left",  false };
		Audio_in::Connection  _right { _env, "right", false };

		void _play_packet()
		{
			Stream *left  =  _left.stream();
			Stream *right = _right.stream();

			auto const pos = left->pos();
			Packet *p_left  =  left->get(pos);
			Packet *p_right = right->get(pos);

			/* convert float to S16LE */
			static short data[2 * PERIOD];

			if (p_left->valid() && p_right->valid()) {

				for (unsigned i = 0; i < 2 * PERIOD; i += 2) {
					data[i] = p_left->content()[i / 2] * 32767;
					data[i + 1] = p_right->content()[i / 2] * 32767;
				}

				/* blocking-write packet to ALSA */
				while (audio_drv_play(data, PERIOD)) {
					/* try to restart the driver silently */
					audio_drv_stop();
					audio_drv_start();
				}
			}

			 left->increment_position();
			right->increment_position();
		}

		Genode::Signal_handler<Out> _progress_handler {
			_env.ep(), *this, &Out::_play_packet };

	public:

		Out(Genode::Env &env)
		: _env(env)
		{
			_left.start();
			_right.start();
			_left.progress_sigh(_progress_handler);
		}
};


struct Alsa_driver::Main
{
	Genode::Env  &env;

	Genode::Attached_rom_dataspace config { env, "config" };

	Playback::Out out { env };

	Main(Genode::Env &env) : env(env)
	{
		char dev[32] = { 'h', 'w', 0 };
		try {
			config.xml().attribute("alsa_device").value(dev, sizeof(dev));
		} catch (...) { }

		/* init ALSA */
		int err = audio_drv_init(dev);
		if (err) {
			if (err == -1) {
				Genode::error("could not open ALSA device ", Genode::Cstring(dev));
			} else {
				Genode::error("could not initialize driver error ", err);
			}

			throw -1;
		}
		audio_drv_start();
	}
};


/***************
 ** Component **
 ***************/

void Component::construct(Genode::Env &env) { static Alsa_driver::Main main(env); }
