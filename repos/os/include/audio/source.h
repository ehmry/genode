/*
 * \brief  Audio session producer class
 * \author Emery Hemingway
 * \date   2019-06-13
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* Genode includes */
#include <audio_out_session/connection.h>

namespace Audio
{
	struct Source;
	class Stereo_out;
};


struct Audio::Source
{
	virtual ~Source() { }
	virtual bool fill(float *left, float *right, Genode::size_t samples) = 0;
};


class Audio::Stereo_out
{
	private:

		Audio::Source &_source;

		Audio_out::Connection _left, _right;

		Genode::Io_signal_handler<Stereo_out> _progress_handler;

	public:

		Stereo_out(Genode::Env &env, Audio::Source &source)
		: _source(source)
		, _left (env,  "left", false, false)
		, _right(env, "right", false, false)
		, _progress_handler(env.ep(), *this, &Stereo_out::progress)
		{
			_left.progress_sigh(_progress_handler);
		}

		void start()
		{
			 _left.start();
			_right.start();
		}

		void stop()
		{
			 _left.stop();
			_right.stop();
		}

		void progress()
		{
			using namespace Audio_out;

			while (!_left.stream()->full()) {
				Packet  *left =  _left.stream()->alloc();
				unsigned  pos =  _left.stream()->packet_position(left);
				Packet *right = _right.stream()->get(pos);

				if (!_source.fill(left->content(), right->content(), PERIOD))
					break;

				 _left.submit(left);
				_right.submit(right);
			}
		}
};
