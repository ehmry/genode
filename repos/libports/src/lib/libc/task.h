/*
 * \brief  Libc-internal kernel API
 * \author Christian Helmuth
 * \author Emery Hemingway
 * \date   2016-12-14
 *
 * TODO document libc tasking including
 * - the initial thread (which is neither component nor pthread)
 *   - processes incoming signals and forwards to entrypoint
 * - the main thread (which is the kernel and the main user context)
 * - pthreads (which are pthread user contexts only)
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__TASK_H_
#define _LIBC__TASK_H_

#include <libc/component.h>

namespace Libc {

	/**
	 * Get time since startup in ms
	 */
	unsigned long current_time();

	/**
	 * Suspend main user context and the component entrypoint
	 *
	 * This interface is solely used by the implementation of fork().
	 */
	void schedule_suspend(void (*suspended) ());

	struct Select_handler_base;

	/**
	 * Schedule select handler that is deblocked by ready fd sets
	 */
	void schedule_select(Select_handler_base *);
}

#endif /* _LIBC__TASK_H_ */
