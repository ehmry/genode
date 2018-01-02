/*
 * \brief  Bulk TCP stream test
 * \author Emery Hemingway
 * \date   2018-01-02
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/thread.h>
#include <util/string.h>
#include <nic/packet_allocator.h>

extern "C" {
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
}

/* PCG includes */
#include <pcg_variants.h>

/**
 * Handle bi-directional byte streams.
 */
void stream(int conn, bool initiate) {
	uint8_t buf[1024];
	ssize_t buflen;
	size_t nr = 0;

	/* deterministic byte streams */
	pcg8i_random_t pcg_local, pcg_remote;
	{
		uint64_t pcg_init[2] = PCG32_INITIALIZER;
		pcg8i_srandom_r(&pcg_local, pcg_init[0], pcg_init[1]);
		pcg8i_srandom_r(&pcg_remote, pcg_init[0], pcg_init[1]);
	}

	if (initiate) {
		buflen = sizeof(buf);
		for (ssize_t i = 0; i < buflen; ++i) {
			buf[i] = pcg8i_random_r(&pcg_local);
		}
		send(conn, buf, buflen, 0);
	}

	while (true) {
		buflen = recv(conn, buf, sizeof(buf), 0);
		for (ssize_t i = 0; i < buflen; ++i) {
			++nr;
	
			if (buf[i] != pcg8i_random_r(&pcg_remote)) {
				Genode::error("stream corrupted at byte ", nr, " ",
				              (Genode::Hex)buf[i]);
				return;
			}
		}

		Genode::log("processed ", nr, " bytes");

		buflen = sizeof(buf);
		for (ssize_t i = 0; i < buflen; ++i) {
			buf[i] = pcg8i_random_r(&pcg_local);
		}

		send(conn, buf, buflen, 0);
	}
}

int main(int argc, char const **argv)
{
	int s;
	bool initiate = false;

	if (argc != 4) {
		Genode::error("invalid arguments, must be '-c|-s IPADDR PORT'");
	}

	if (strcmp(argv[1], "-c") == 0) {
		initiate = true;
	} else
	if (strcmp(argv[1], "-s") == 0) {
		initiate = false;
	} else {
		Genode::log("invalid arguments");
		return -1;
	}

	Genode::log("Create new socket ...");
	if((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		Genode::error("no socket available!");
		return -1;
	}

	struct sockaddr_in in_addr;
	in_addr.sin_family = AF_INET;
	in_addr.sin_addr.s_addr = inet_addr(argv[2]);
	in_addr.sin_port = htons(atoi(argv[3]));

	if (initiate) {
		Genode::log("Now, I will connect ...");
		if (connect(s, (struct sockaddr*)&in_addr, sizeof(in_addr))) {
			Genode::error("connect failed!");
			return -1;
		}
	} else {
		Genode::log("Now, I will bind ...");
		if(bind(s, (struct sockaddr*)&in_addr, sizeof(in_addr))) {
			Genode::error("bind failed!");
			return -1;
		}

		Genode::log("Now, I will listen ...");
		if(listen(s, 5)) {
			Genode::error("listen failed!");
			return -1;
		}
	}

	Genode::log("Start the loop ...");
	if (initiate) {
		stream(s, true);
	} else {
		while(true) {
			struct sockaddr addr;
			socklen_t len = sizeof(addr);
			int client = accept(s, &addr, &len);
			if(client < 0) {
				Genode::warning("invalid socket from accept!");
				continue;
			}
			stream(client, false);
			close(client);
		}
	}
	return 0;
}
