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

namespace Libc {

	void task_suspend();
	void task_resume();

}

#endif