#ifndef _INCLUDE__NICHTS_STORE_SESSION__CLIENT_H_
#define _INCLUDE__NICHTS_STORE_SESSION__CLIENT_H_

#include <base/rpc_client.h>
#include <nix_store_session/nix_store_session.h>


namespace Nichts_store {

	struct Session_client : Genode::Rpc_client<Session>
	{
		Session_client(Genode::Capability<Session> cap)
		: Genode::Rpc_client<Session>(cap) { }

		void realise(Path const  &drvPath, Mode mode = NORMAL)
		{
			call<Rpc_realise>(drvPath, mode);
		}
	};

}

#endif