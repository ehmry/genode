/*
 * \brief  Common format parameters for Audio_* sessions
 * \author Emery Hemingway
 * \date   2019-06-19
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__AUDIO__PARAMETERS_H_
#define _INCLUDE__AUDIO__PARAMETERS_H_

#include <base/stdint.h>

#define GENODE_AUDIO_SAMPLE_RATE 48000

#ifdef __cplusplus

namespace Audio {

	enum {
		QUEUE_SIZE  = 431,
		SAMPLE_RATE = GENODE_AUDIO_SAMPLE_RATE,
		SAMPLE_SIZE = sizeof(float),
	};

	static constexpr Genode::size_t PERIOD = 800; /* samples per period (60 Hz) */
}

#endif

#endif // _INCLUDE__AUDIO__PARAMETERS_H_
