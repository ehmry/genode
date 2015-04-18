/*
 * \brief  Client-side Nichts_store session interface
 * \author Emery Hemingway
 * \date   2015-03-07
 */

#ifndef _INCLUDE__NICHTS_STORE_SESSION__CLIENT_H_
#define _INCLUDE__NICHTS_STORE_SESSION__CLIENT_H_

#include <base/rpc_client.h>
#include <nichts_store_session/nichts_store_session.h>


namespace Nichts_store {

	struct Session_client : Genode::Rpc_client<Session>
	{
		Session_client(Genode::Capability<Session> cap)
		: Genode::Rpc_client<Session>(cap) { }

		Path hash(File_system::Node_handle node)
		{
			return call<Rpc_hash>(node);
		}

		void realise(Path const  &drvPath, Mode mode = NORMAL)
		{
			call<Rpc_realise>(drvPath, mode);
		}
	};

}

#endif