#ifndef _INCLUDE__NICHTS_STORE_SESSION__CONNECTION_H_
#define _INCLUDE__NICHTS_STORE_SESSION__CONNECTION_H_

#include <nichts_store_session/client.h>
#include <base/connection.h>


namespace Nichts_store {

	struct Connection : Genode::Connection<Session>, Session_client
	{
		Connection()
		:
			Genode::Connection<Session>(session("ram_quota=1M")),
			Session_client(cap())
		{}
	};

}

#endif