/*
 * \brief  Connection to block service
 * \author Stefan Kalkowski
 * \date   2010-07-07
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BLOCK_SESSION__CONNECTION_H_
#define _INCLUDE__BLOCK_SESSION__CONNECTION_H_

#include <block_session/client.h>
#include <base/connection.h>
#include <base/allocator.h>

namespace Block {
	struct Connection;

	/* recommended packet transmission buffer size */
	enum { DEFAULT_TX_BUF_SIZE = 128*1024 };
}

struct Block::Connection : Genode::Connection<Session>, Session_client
{
	/**
	 * Issue session request
	 *
	 * \noapi
	 */
	Capability<Block::Session> _session(Genode::Parent &parent,
	                                    Genode::size_t tx_buf_size,
	                                    bool read, bool write,
	                                    sector_t offset, sector_t span,
	                                    char const *label)
	{
		return session(parent,
		               "ram_quota=%zd, tx_buf_size=%zd, "
		               "readable=%d, writeable=%d, offset=%llu, span=%llu, label=\"%s\"",
		               3*4096 + tx_buf_size, tx_buf_size, read, write, offset, span, label);
	}

	/**
	 * Constructor
	 *
	 * \param tx_buffer_alloc  allocator used for managing the
	 *                         transmission buffer
	 * \param tx_buf_size      size of transmission buffer in bytes
	 */
	Connection(Genode::Env             &env,
	           Genode::Range_allocator *tx_block_alloc,
	           Genode::size_t           tx_buf_size = DEFAULT_TX_BUF_SIZE,
	           sector_t                 offset = 0, /* session offset */
	           sector_t                 span   = 0, /* session span constraint */
	           bool                     readable  = true,
	           bool                     writeable = true,
	           const char              *label  = "")
	:
		Genode::Connection<Session>(env, _session(
			env.parent(), tx_buf_size, readable, writeable, offset, span,  label)),
		Session_client(cap(), tx_block_alloc)
	{ }

	/**
	 * Constructor
	 *
	 * \noapi
	 * \deprecated  Use the constructor with 'Env &' as first
	 *              argument instead
	 */
	Connection(Genode::Range_allocator *tx_block_alloc,
	           Genode::size_t           tx_buf_size = DEFAULT_TX_BUF_SIZE,
	           sector_t                 offset = 0, /* session offset */
	           sector_t                 span   = 0, /* session span constraint */
	           bool                     readable  = true,
	           bool                     writeable = true,
	           const char              *label  = "")
	:
		Genode::Connection<Session>(_session(
			*Genode::env()->parent(), tx_buf_size, readable, writeable, offset, span, label)),
		Session_client(cap(), tx_block_alloc)
	{ }
};

#endif /* _INCLUDE__BLOCK_SESSION__CONNECTION_H_ */
