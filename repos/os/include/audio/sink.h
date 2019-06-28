/*
 * \brief  Audio session consumer class
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
#include <audio_in_session/connection.h>

namespace Audio
{
	struct Sink;
	class Stereo_in;
};


struct Audio::Sink
{
	virtual ~Sink() { }
	virtual bool drain(float const *left, float const *right, Genode::size_t samples) = 0;
};


class Audio::Stereo_in
{
	private:

		Audio::Sink &_sink;

		Audio_in::Connection _left, _right;

		Genode::Signal_handler<Stereo_in> _reset_handler;

		Genode::Signal_context_capability _progress_sigh { };

		void _handle_reset()
		{
			_progress_sigh = _left.progress_sigh();
			if (_progress_sigh.valid())
				Genode::Signal_transmitter(_progress_sigh).submit();
		}

	public:

		Stereo_in(Genode::Env &env, Sink &sink)
		: _sink(sink), _left (env, "left"), _right(env, "right"),
		  _reset_handler(env.ep(), *this, &Stereo_in::_handle_reset)
		{
			_left.reset_sigh(_reset_handler);
			_handle_reset();
		}

		/**
		 * Drain incoming audio from packet stream and
		 * transmit a progress signal to the source.
		 */
		bool progress()
		{
			using namespace Audio_in;
			bool res = false;

			while (!_left.stream().empty()) {
				unsigned  pos =  _left.stream().play_pos();
				Packet  *left =  _left.stream().get(pos);
				Packet *right = _right.stream().get(pos);

				if (!_sink.drain(left->content(), right->content(), PERIOD))
					break;

				 left->played();
				right->played();
				 _left.stream().increment_position();
				_right.stream().increment_position();

				res = true;
			}

			{
				if (_progress_sigh.valid())
					Genode::Signal_transmitter(_progress_sigh).submit();
			}

			return res;
		}
};
