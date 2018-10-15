/*
 * \brief  Getaddrinfo implementation
 * \author Emery Hemingway
 * \date   2018-07-12
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/debug.h>

#include <base/log.h>

extern "C" {

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int getaddrinfo(const char *, const char *,
                const struct addrinfo *, struct addrinfo **)
{
	PDBG();
	return -1;
}


void
freeaddrinfo(struct addrinfo */*ai*/)
{
	PDBG();
}

} /* extern "C" */
