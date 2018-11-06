/*
 * \brief  Rng session interface
 * \author Emery Hemingway
 * \date   2018-11-06
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RNG_SESSION__RNG_SESSION_H_
#define _INCLUDE__RNG_SESSION__RNG_SESSION_H_

#include <session/session.h>
#include <base/rpc.h>
#include <base/stdint.h>

namespace Rng { struct Session; }


struct Rng::Session : Genode::Session
{
	/**
	 * \noapi
	 */
	static const char *service_name() { return "Rng"; }

	enum { CAP_QUOTA = 2 };

	virtual Genode::uint64_t gather() = 0;

	GENODE_RPC(Rpc_gather, Genode::uint64_t, gather);
	GENODE_RPC_INTERFACE(Rpc_gather);
};

#endif /* _INCLUDE__RNG_SESSION__RNG_SESSION_H_ */
