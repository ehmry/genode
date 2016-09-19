/*
 * \brief  Libc task switching
 * \author Christian Helmuth
 * \author Emery Hemingway
 * \date   2016-10-25
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LIBC__TASK_H_
#define _LIBC__TASK_H_

#include <base/debug.h>

extern "C" void wait_for_continue(void);

namespace Libc {

	/**
	 * Task represents a thread of user execution.
	 * When a blocking call is made a task will suspend
	 * until unblocked by the Libc "kernel".
	 */
	struct Task
	{
		/* Suspend task execution */
		virtual void   block() = 0;

		/* Resume task execution */
		virtual void unblock() = 0;
	};

	Task &this_task();

}

#endif