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

namespace Audio {
	class Packet;
	class Stream;
	struct Stream_source;
	struct Stream_sink;

	enum {
		QUEUE_SIZE  = 431,            /* buffer queue size (~5s) */
		SAMPLE_RATE = 48000,
		SAMPLE_SIZE = sizeof(float),
		OVERRUN_THRESHOLD = QUEUE_SIZE >> 3
			/* standard threshold for sending overrun signals */
	};

	static constexpr Genode::size_t PERIOD = 512; /* samples per periode (~11.6ms) */
}


/**
 * Audio packet containing frames
 */
class Audio::Packet
{
	private:

		friend class Stream_sink;

		bool  _valid;
		bool  _wait_for_record;
		float _data[PERIOD];

		void _submit() { _valid = true; _wait_for_record = true; }
		void _alloc() { _wait_for_record = false; _valid = false; }

	public:

		Packet() : _valid(false), _wait_for_record(false) { }

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

		/**
		 * Record state
		 *
		 * \return  true if the packet has been recorded; false otherwise
		 */
		bool recorded() const { return !_wait_for_record; }

		/**
		 * Valid state
		 *
		 * The valid state of a packet describes that the packet has been
		 * processed by the server even though it may not have been played back
		 * if the packet is invalid. For example, if a server is a filter, the
		 * audio may not have been processed by the output driver.
		 *
		 * \return  true if packet has *not* been processed yet; false otherwise
		 */
		bool valid() const { return _valid; }

		Genode::size_t size() const { return sizeof(_data); }


		/**********************************************
		 ** Intended to be called by the server side **
		 **********************************************/

		/**
		 * Invalidate packet, thus marking it as processed
		 */
		void invalidate() { _valid = false; }

		/**
		 * Mark a packet as recorded
		 */
		void mark_as_recorded() { _wait_for_record = false; }
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

		Genode::uint32_t  _record_pos { 0 }; /* current record (server) position */
		Genode::uint32_t  _play_pos   { 0 }; /* current playback (client) position */

		Packet    _buf[QUEUE_SIZE]; /* packet queue */

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
				valid |= _buf[i].valid();

			return !valid;
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
	Packet *next(Packet *packet)
	{
		return packet
			? get(packet_position(packet) + 1)
			: get(play_pos() + 1);
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
		return packet
			? get(packet_position(packet) + 1)
			: get(_record_pos + 1);
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

	/**
	 * Submit a packet to the packet queue
	 */
	void submit(Packet *p) { p->_submit(); }
};

#endif /* _INCLUDE__AUDIO__STREAM_H_ */