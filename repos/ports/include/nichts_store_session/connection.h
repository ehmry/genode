#ifndef _INCLUDE__NICHTS_STORE_SESSION__CONNECTION_H_
#define _INCLUDE__NICHTS_STORE_SESSION__CONNECTION_H_

#include <nichts_store_session/client.h>
#include <base/connection.h>
#include <base/allocator.h>

namespace Nichts_store {

	struct Connection : Genode::Connection<Session>, Session_client
	{
		/**
		 * Constructor
		 *
		 * \param tx_buffer_alloc  allocator used for managing the
		 *                         transmission buffer
		 * \param tx_buf_size      size of transmission buffer in bytes
		 */
		Connection(Range_allocator &tx_block_alloc,
		           size_t           tx_buf_size = 128*1024,
		           const char      *label = "")
		:
			Genode::Connection<Session>(
				session("ram_quota=%zd, tx_buf_size=%zd, label=\"%s\"",
				        16*1024*sizeof(long) + tx_buf_size, tx_buf_size, label)),
			Session_client(cap(), tx_block_alloc)
		{}
	};

}

#endif