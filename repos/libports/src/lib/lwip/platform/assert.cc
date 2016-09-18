/*
 * \brief  Print function for debugging functionality of lwIP.
 * \author Stefan Kalkowski
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/sleep.h>

extern "C" {

/* LwIP includes */
#include <arch/cc.h>

	void lwip_platform_assert(char const* msg, char const *file, int line)
	{
		Genode::error("Assertion \"", msg, "\" ", file, ":", line);
		Genode::sleep_forever();
	}

}
