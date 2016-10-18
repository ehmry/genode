/*
 * \brief  RFC862 echo server
 * \author Emery Hemingway
 * \date   2016-10-17
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


/* Genode includes */
#include <base/log.h>

/* libc includes */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define ECHO_PORT 7 
#define MAXBUFLEN 0xFFFF
#define RECV_FLAGS 0
#define SEND_FLAGS 0

static void print(Genode::Output &output, sockaddr_in const &addr)
{
	print(output, (ntohl(addr.sin_addr.s_addr) >> 24) & 0xff);
	output.out_string(".");
	print(output, (ntohl(addr.sin_addr.s_addr) >> 16) & 0xff);
	output.out_string(".");
	print(output, (ntohl(addr.sin_addr.s_addr) >>  8) & 0xff);
	output.out_string(".");
	print(output, (ntohl(addr.sin_addr.s_addr) >>  0) & 0xff);
	output.out_string(":");
	print(output, ntohs(addr.sin_port));
}

int main(void)
{
	int udp_sock;
	int rv = 0;
	ssize_t numbytes;
	struct sockaddr_in their_addr;
	char buf[MAXBUFLEN];
	socklen_t addr_len;

	udp_sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

	struct sockaddr_in const addr = { 0, AF_INET, htons(ECHO_PORT), { INADDR_ANY } };

	if (bind(udp_sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		close(udp_sock);
		Genode::log("bind failed");
		return errno;
	}

	for (;;) {
		addr_len = sizeof their_addr;
		numbytes = recvfrom(udp_sock, buf, sizeof(buf), RECV_FLAGS,
		                    (struct sockaddr *)&their_addr, &addr_len);
		if (numbytes == -1) {
			rv = errno;
			perror("recvfrom failed");
			break;
		}

		Genode::log("received ",numbytes," bytes from ", their_addr);

		numbytes = sendto(udp_sock, buf, numbytes, SEND_FLAGS,
		                   (struct sockaddr *)&their_addr, addr_len);
		if (numbytes == -1) {
			rv = errno;
			perror("sendto failed");
			break;
		}

		Genode::log("sent ",numbytes," bytes to ", their_addr);
	}

	close(udp_sock);
	return rv;
}