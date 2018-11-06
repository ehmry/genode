/*
 * \brief  Connection to Rng service
 * \author Emery Hemingway
 * \date   2018-11-06
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RNG_SESSION__CONNECTION_H_
#define _INCLUDE__RNG_SESSION__CONNECTION_H_

#include <rng_session/client.h>
#include <base/connection.h>

namespace Rng { struct Connection; }


struct Rng::Connection : Genode::Connection<Session>, Session_client
{
	/**
	 * Constructor
	 */
	Connection(Genode::Env &env, char const *label = "")
	:
		Genode::Connection<Rng::Session>(
			env, session(env.parent(), "ram_quota=8K, label=\"%s\"", label)),
		Session_client(cap())
	{ }
};

#endif /* _INCLUDE__RNG_SESSION__CONNECTION_H_ */
