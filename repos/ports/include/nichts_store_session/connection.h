/*
 * \brief  Connection to Nichts_store service
 * \author Emery Hemingway
 * \date   2015-03-07
 */

#ifndef _INCLUDE__NICHTS_STORE_SESSION__CONNECTION_H_
#define _INCLUDE__NICHTS_STORE_SESSION__CONNECTION_H_

#include <nichts_store_session/client.h>
#include <base/connection.h>
#include <file_system_session/client.h>


namespace Nichts_store {

	class Connection : public Genode::Connection<Session>, public Session_client
	{
		private:

			File_system::Session_client _fs;

		public:

			/**
			 * Constructor
			 *
			 * \param tx_buffer_alloc  allocator used for managing the
			 *                         file system transmission buffer
			 * \param tx_buf_size      size of file system transmission buffer in bytes
			 */
			Connection(Range_allocator &tx_block_alloc,
			           size_t           tx_buf_size = 128*1024,
			           const char      *label = "")
			:
				Genode::Connection<Session>(
					session("ram_quota=%zd, tx_buf_size=%zd, label=\"%s\"",
					        4*1024*sizeof(long) + tx_buf_size, tx_buf_size, label)),

				Session_client(cap()),

				/* Request file system sub session. */
				_fs(file_system_session(), tx_block_alloc)
			{}

			/**
			 * Return sub session for the store virtual filesystem.
			 */
			File_system::Session *file_system() { return &_fs; };
	};

}

#endif