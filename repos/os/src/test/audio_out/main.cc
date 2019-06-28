/*
 * \brief  Audio-out test implementation
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \date   2009-12-03
 *
 * The test program plays several tracks simultaneously to the Audio_out
 * service. See README for the configuration.
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <audio/source.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <dataspace/client.h>
#include <rom_session/connection.h>

using Filename = Genode::String<64>;
using namespace Genode;
using namespace Audio_out;

static constexpr bool const verbose = true;


struct Track final : Audio::Source
{
	/*
	 * Noncopyable
	 */
	Track(Track const &);
	Track &operator = (Track const &);

	enum {
		CHN_CNT      = 2,                      /* number of channels */
		FRAME_SIZE   = sizeof(float),
		PERIOD_CSIZE = FRAME_SIZE * PERIOD,    /* size of channel packet (bytes) */
		PERIOD_FSIZE = CHN_CNT * PERIOD_CSIZE, /* size of period in file (bytes) */
	};

	Env & _env;

	Filename const & _name;

	Attached_rom_dataspace _sample_ds { _env, _name.string() };

	char     const * const _base   { _sample_ds.local_addr<char const>() };
	size_t           const _size   { _sample_ds.size() };
	size_t                 _offset { 0 };

	Audio::Stereo_out _stereo_out { _env, *this };

	bool fill(float *left, float *right, Genode::size_t /*samples*/) override
	{
		if (_size <= _offset) {
			_stereo_out.stop();
			log("played '", _name, "' 1 time(s)");
			return false;
		}

		/*
		 * The current chunk (in number of frames of one channel)
		 * is the size of the period except at the end of the
		 * file.
		 */
		size_t chunk = (_offset + PERIOD_FSIZE > _size)
		               ? (_size - _offset) / CHN_CNT / FRAME_SIZE
		               : PERIOD;

		/* copy channel contents into sessions */
		float *content = (float *)(_base + _offset);
		for (unsigned c = 0; c < CHN_CNT * chunk; c += CHN_CNT) {
			 left[c / 2] = content[c + 0];
			right[c / 2] = content[c + 1];
		}

		/* handle last packet gracefully */
		if (chunk < PERIOD) {
			memset( left + chunk, 0, PERIOD_CSIZE - FRAME_SIZE * chunk);
			memset(right + chunk, 0, PERIOD_CSIZE - FRAME_SIZE * chunk);
		}

		_offset += PERIOD_FSIZE;
		return true;
	}

	Track(Env & env, Filename const & name)
	: _env(env), _name(name)
	{
		if (verbose)
			log(_name, " size is ", _size, " bytes "
			    "(attached to ", (void *)_base, ")");

		_stereo_out.start();
	}

	~Track() { }
};


struct Main
{
	enum { MAX_FILES = 16 };

	Env &                  env;
	Heap                   heap { env.ram(), env.rm() };
	Attached_rom_dataspace config { env, "config" };
	Filename               filenames[MAX_FILES];
	unsigned               track_count = 0;

	void handle_config();

	Main(Env & env);
};


void Main::handle_config()
{
	try {
		config.xml().for_each_sub_node("filename",
		                               [this] (Xml_node const & node)
		{
			if (!(track_count < MAX_FILES)) {
				warning("test supports max ", (int)MAX_FILES,
				        " files. Skipping...");
				return;
			}

			node.with_raw_content([&] (char const *start, size_t length) {
				filenames[track_count++] = Filename(Cstring(start, length)); });
		});
	}
	catch (...) {
		warning("couldn't get input files, failing back to defaults");
		filenames[0] = Filename("1.raw");
		filenames[1] = Filename("2.raw");
		track_count  = 2;
	}
}


Main::Main(Env & env) : env(env)
{
	log("--- Audio_out test ---");

	handle_config();

	for (unsigned i = 0; i < track_count; ++i)
		new (heap) Track(env, filenames[i]);
}


void Component::construct(Env & env) { static Main main(env); }
