/*
 * \brief  Types used by VFS server
 * \author Emery Hemingway
 * \date   2016-07-27
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#ifndef __SERVER__VFS__TYPES_H_
#define __SERVER__VFS__TYPES_H_

#include <vfs/types.h>

namespace Vfs_server {
	typedef Genode::Path<Vfs::MAX_PATH_LEN> Path;
}

#endif