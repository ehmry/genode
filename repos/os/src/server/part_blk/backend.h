/*
 * \brief  Block-session component for partition server
 * \author Stefan Kalkowski
 * \author Emery Hemingway
 * \date   2013-12-04
 */

/*
 * Copyright (C) 2013-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PART_BLK__BACKEND_H_
#define _PART_BLK__BACKEND_H_

#include <base/exception.h>
#include <base/component.h>
#include <base/service.h>
#include <os/session_policy.h>
#include <root/component.h>
#include <block_session/rpc_object.h>


namespace Block { class Backend; }

class Block::Backend : public Block::Connection
{
	private:

		sector_t       _blk_cnt  = 0;
		Genode::size_t _blk_size = 0;

	public:

		Backend(Genode::Env &env, Genode::Range_allocator &alloc)
		: Connection(env, &alloc, DEFAULT_TX_BUF_SIZE/2, 0, 0, true, false)
		{
	        Operations ops;
			info(&_blk_cnt, &_blk_size, &ops);
		}

		sector_t       blk_cnt()  const { return _blk_cnt; }
		Genode::size_t blk_size() const { return _blk_size;  }
};

#endif
