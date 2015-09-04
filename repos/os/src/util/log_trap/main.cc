/*
 * \brief  Utility for monitoring a child
 * \author Emery Hemingway
 * \date   2015-08-26
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/config.h>
#include <base/printf.h>
#include <base/env.h>
#include <base/sleep.h>

/* Local includes */
#include "child.h"

using namespace Genode;

int main(int, char **)
{
	static Log_trap::Child child(config()->xml_node().sub_node("start"));

	/* never exit on this thread, lie in wait for the child */
	sleep_forever();
	return ~0;
}