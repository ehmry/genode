/*
 * \brief  Client-side Rng session interface
 * \author Emery Hemingway
 * \date   2018-11-06
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RNG_SESSION__CLIENT_H_
#define _INCLUDE__RNG_SESSION__CLIENT_H_

#include <rng_session/rng_session.h>
#include <base/rpc_client.h>

namespace Rng { struct Session_client; }


struct Rng::Session_client : Genode::Rpc_client<Session>
{
	Session_client(Genode::Capability<Session> cap)
	: Genode::Rpc_client<Session>(cap) {}

	Genode::uint64_t gather() { return call<Rpc_gather>(); }
};

#endif /* _INCLUDE__RNG_SESSION__CLIENT_H_ */
