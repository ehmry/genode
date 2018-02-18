/*
 * \brief  Libc getaddrinfo(...) test
 * \author Emery Hemingway
 * \date   2018-02-18
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int main(int argc, char **argv)
{
	struct addrinfo hints;
	char ipstr[INET6_ADDRSTRLEN];

	int i;
	for (i = 1; i < argc; ++i) {
		int res;
		char const *arg = argv[i];

		struct addrinfo *info, *p;

		memset(&hints, 0x00, sizeof(hints));
		hints.ai_family = AF_UNSPEC;

		res = getaddrinfo(arg, NULL, &hints, &info);
		if (res != 0) {
			fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(res));
			continue;
		}

		for (p = info; p != NULL; p = p->ai_next) {
			void *addr;

			// get the pointer to the address itself,
			// different fields in IPv4 and IPv6:
			if (p->ai_family == AF_INET) { // IPv4
				struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
				addr = &(ipv4->sin_addr);
			} else { // IPv6
				struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
				addr = &(ipv6->sin6_addr);
			}

			// convert the IP to a string and print it:
			inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
			printf("%s: %s\n", arg, ipstr);
		}

		freeaddrinfo(info);
	}

	return 0;
}
