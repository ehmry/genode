/*
 * \brief  Runtime state of the RAM file system
 * \author Norman Feske
 * \date   2018-05-15
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _RAM_FS_STATE_H_
#define _RAM_FS_STATE_H_

/* Genode includes */
#include <util/xml_node.h>

/* local includes */
#include "types.h"

namespace Sculpt_manager { struct Ram_fs_state; }

struct Sculpt_manager::Ram_fs_state : Noncopyable
{
	static Ram_quota initial_ram() { return Ram_quota{1024*1024}; }

	Ram_quota ram_quota = initial_ram();

	struct Version { unsigned value; } version { 0 };

	bool inspected = false;
};

#endif /* _RAM_FS_STATE_H_ */
