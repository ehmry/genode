/*
 * \brief  Audio channel selector utilities
 * \author Emery Hemingway
 * \date   2018-05-29
 */

namespace Audio {
	struct Channels;
};

/**
 * Value object carrying a bitmask of channel selectors
 */
struct Audio::Channels
{
	unsigned value = 0;

	enum {
		LEFT   = 1 << 0,
		RIGHT  = 1 << 1,
		STEREO = LEFT|RIGHT
	};

	bool single() const
	{
		bool r = false;

		for (unsigned i = 0; i < sizeof(value)*8-1; ++i) {
			if ((value >> i) & 1) {
				if (r) return false;
				r = true;
			}
		}
		return r;
	}

	explicit Channels(unsigned value) : value(value) { }
};
