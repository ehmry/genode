#ifndef _INCLUDE__NICHTS_STORE_SESSION__CLIENT_H_
#define _INCLUDE__NICHTS_STORE_SESSION__CLIENT_H_

#include <base/rpc_client.h>
#include <nichts_store_session/capability.h>
#include <packet_stream_tx/client.h>
#include <file_system_session/file_system_session.h>


namespace Nichts_store {

	class Session_client : public Genode::Rpc_client<Session>
	{
		private:

			Packet_stream_tx::Client<Tx> _tx;

		public:

			/**
			 * Constructor
			 *
			 * \param session          session capability
			 * \param tx_buffer_alloc  allocator used for managing the
			 *                         transmission buffer
			 */
			Session_client(Session_capability  session,
			               Range_allocator    &tx_buffer_alloc)
			:
				Rpc_client<Session>(session),
				_tx(call<Rpc_tx_cap>(), &tx_buffer_alloc)
			{ }

			/****************************
			 ** Nichts store interface **
			 ****************************/

			Tx::Source *tx() { return _tx.source(); }

			void realise(Path const  &drvPath, Mode mode = NORMAL)
			{
				call<Rpc_realise>(drvPath, mode);
			}

			File_system::File_handle add_file(File_system::Name const &name)
			{
				return call<Rpc_add_file>(name);
			}

			void close_file(File_system::File_handle handle)
			{
				call<Rpc_close_file>(handle);
			}
	};

}

#endif