/*
 * \brief  Common definitions for Audio_* sessions
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Emery Hemingway
 * \date   2018-06-05
 *
 * An Audio_in session corresponds to one input channel, which can be used to
 * receive audio frames. Each session consists of an 'Audio_in::Stream' object
 * that resides in shared memory between the client and the server. The
 * 'Audio_in::Stream' in turn consists of 'Audio_in::Packet's that contain
 * the actual frames. Each packet within a stream is freely accessible. When
 * recording the source will allocate a new packet and override already
 * recorded ones if the queue is already full. In contrast to the
 * 'Audio_out::Stream' the current position pointer is updated by the client.
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__AUDIO__STREAM_H_
#define _INCLUDE__AUDIO__STREAM_H_

#include <base/debug.h>

namespace Audio {
	class Packet;
	class Stream;
	struct Stream_source;
	struct Stream_sink;

	enum {
		QUEUE_SIZE  = 431,            /* buffer queue size (~5s) */
		SAMPLE_RATE = 48000,
		SAMPLE_SIZE = sizeof(float),
		UNDERRUN_THRESHOLD = QUEUE_SIZE >> 3,
		OVERRUN_THRESHOLD = QUEUE_SIZE - UNDERRUN_THRESHOLD
			/* standard threshold for sending overrun signals */
	};

	static constexpr Genode::size_t PERIOD = 512; /* samples per periode (~11.6ms) */
}


/**
 * Audio packet containing frames
 */
class Audio::Packet
{
	friend class Stream;
	friend class Stream_sink;
	friend class Stream_source;

	protected:

		float _data[PERIOD];
		bool  _active = false;

	public:

		Packet() { }

		void recorded() { _active = true; }
		void played() { _active = false; }

		/**
		 * Copy data into packet, if there are less frames given than 'PERIOD',
		 * the remainder is filled with zeros
		 *
		 * \param  data  frames to copy in
		 * \param  size  number of frames to copy
		 */
		void content(float *data, Genode::size_t samples)
		{
			using namespace Genode;

			memcpy(_data, data, (samples > PERIOD ? (size_t)PERIOD : samples) * SAMPLE_SIZE);

			if (samples < PERIOD)
				memset(data + samples, 0, (PERIOD - samples) * SAMPLE_SIZE);
		}

		/**
		 * Get content
		 *
		 * \return  pointer to frame data
		 */
		float *content() { return _data; }

		Genode::size_t size() const { return sizeof(_data); }
};


/**
 * The audio-stream object containing packets
 *
 * The stream object is created upon session creation. The server will allocate
 * a dataspace on the client's account. The client session will then request
 * this dataspace and both client and server will attach it in their respective
 * protection domain. After that, the stream pointer within a session will be
 * pointed to the attached dataspace on both sides. Because the 'Stream' object
 * is backed by shared memory, its constructor is never supposed to be called.
 */
class Audio::Stream
{
	friend class Stream_source;
	friend class Stream_sink;

	private:

		Packet    _buf[QUEUE_SIZE]; /* packet queue */

		Genode::uint32_t  _record_pos { 0 }; /* current record (server) position */
		Genode::uint32_t  _play_pos   { 0 }; /* current playback (client) position */

	public:

		/**
		 * Exceptions
		 */
		class Alloc_failed { };

		/**
		 * Current record position
		 *
		 * \return  record position
		 */
		unsigned record_pos() const { return _record_pos; }

		/**
		 * Current playback position
		 *
		 * \return  playback position
		 */
		unsigned play_pos() const { return _play_pos; }

		/**
		 * Reset stream queue
		 *
		 * This means that plaback will start at current record position.
		 */
		void reset() { _play_pos = _record_pos; }

		/**
		 * Retrieves the position of a given packet in the stream queue
		 *
		 * \param packet  a packet
		 *
		 * \return  position in stream queue
		 */
		unsigned packet_position(Packet *packet) const { return packet - &_buf[0]; }

		/**
		 * Retrieve an packet at given position
		 *
		 * \param  pos  position in stream
		 *
		 * \return  Audio_in packet
		 */
		Packet *get(unsigned pos) { return &_buf[pos % QUEUE_SIZE]; }

		/**
		 * Check if stream queue is empty
		 */
		bool empty() const
		{
			bool valid = false;
			for (int i = 0; i < QUEUE_SIZE; i++)
				valid |= _buf[i]._active;

			return !valid;
		}

		bool underrun() const
		{
			int gap = _record_pos < _play_pos
				? _record_pos-_play_pos+QUEUE_SIZE
				: _record_pos-_play_pos;
			PDBG("P:", _play_pos, " R:", _record_pos, " gap ", gap);

			if (gap < UNDERRUN_THRESHOLD)
				Genode::error("UNDERRUN");
			return gap < UNDERRUN_THRESHOLD;
		}
};


/**
 * Stream object for retreiving audio packets
 */
struct Audio::Stream_source : Stream
{
	/**
	 * Retrieve next packet for given packet
	 *
	 * \param packet  preceding packet
	 *
	 * \return  Successor of packet or successor of current
	 *          plackbackposition if 'packet' is zero
	 */
	Packet *next(Packet *packet = nullptr)
	{
		Packet *p = get(
			(packet ? packet_position(packet) : play_pos()) + 1);
		return p->_active ? p : nullptr;
	}

	/**
	 * Set current stream position
	 *
	 * \param pos  current position
	 */
	void pos(unsigned p) { _play_pos = p; }

	/**
	 * Increment current stream position by one
	 */
	void increment_position() {
		_play_pos = (_play_pos + 1) % QUEUE_SIZE; }
};


/**
 * Stream object for submitting audio packets
 */
struct Audio::Stream_sink : Stream
{
	friend class Stream;

	/**
	 * Retrieve next packet for given packet
	 *
	 * \param packet  preceding packet
	 *
	 * \return  Successor of packet or successor of current
	 *           position if 'packet' is zero
	 */
	Packet *next(Packet *packet = nullptr)
	{
		Packet *p = get(
			(packet ? packet_position(packet) : record_pos()) + 1);
		return p->_active ? nullptr : p;
	}

	/**
	 * Set current stream position
	 *
	 * \param pos  current position
	 */
	void pos(unsigned p) { _record_pos = p; }

	/**
	 * Increment current stream position by one
	 */
	void increment_position() {
		_record_pos = (_record_pos + 1) % QUEUE_SIZE; }
};

#endif /* _INCLUDE__AUDIO__STREAM_H_ */