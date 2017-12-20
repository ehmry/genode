/*
 * \brief  Minimal HTTP server demonstration
 * \author lwIP Team
 * \author Stefan Kalkowski
 * \date   2009-10-23
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <libc/component.h>
#include <nic/packet_allocator.h>
#include <util/string.h>

extern "C" {
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
}

using Response = Genode::String<1024>;


/**
 * Handle a single client's request.
 *
 * \param conn  socket connected to the client
 */
void http_server_serve(int conn, Response & response)
{
	char    buf[1024];
	ssize_t buflen;

	/* Read the data from the port, blocking if nothing yet there.
	   We assume the request (the part we care about) is in one packet */
	buflen = recv(conn, buf, 1024, 0);
	Genode::log("Packet received!");

	/* Ignore all receive errors */
	if (buflen > 0) {

		/* Is this an HTTP GET command? (only check the first 5 chars, since
		   there are other formats for GET, and we're keeping it very simple)*/
		if (buflen >= 5 &&
			buf[0] == 'G' &&
			buf[1] == 'E' &&
			buf[2] == 'T' &&
			buf[3] == ' ' &&
			buf[4] == '/' ) {

			Genode::log("Will send response");

			/* Send our HTML page */
			send(conn, response.string(), response.length(), 0);
		}
	}
}

struct Initialization_failed {};
#define ASSERT(cond, err) \
	if (!cond) { \
		Genode::error(err); \
		throw Initialization_failed(); \
	}

void serve(Libc::Env & env)
{
	using namespace Genode;
	using Address  = Genode::String<16>;

	unsigned port = 0;


	Attached_rom_dataspace config(env, "config");
	Xml_node libc_node = env.libc_config();
	config.xml().attribute("port").value(&port);

	log("Create new socket ...");

	int s;
	ASSERT(((s = socket(AF_INET, SOCK_STREAM, 0)) >= 0),
	       "no socket available!");

	static Response response(
		"HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n"
		"<html><head></head><body>"
		"<h1>HTTP server</h1>"
		"<p>This is a small test page.</body></html>");

	log("Now, I will bind ...");

	struct sockaddr_in in_addr;
	in_addr.sin_family = AF_INET;
	in_addr.sin_port = htons(port);
	in_addr.sin_addr.s_addr = INADDR_ANY;
	ASSERT(!bind(s, (struct sockaddr*)&in_addr, sizeof(in_addr)),
	       "bind failed!");

	log("Now, I will listen ...");
	ASSERT(!listen(s, 5),
	       "listen failed!");

	log("Start the server loop ...");
	while (true) {
		struct sockaddr addr;
		socklen_t len = sizeof(addr);
		int client = accept(s, &addr, &len);
		if(client < 0) {
			warning("invalid socket from accept!");
			continue;
		}
		http_server_serve(client, response);
		close(client);
	}
}

void Libc::Component::construct(Libc::Env &env)
{
	with_libc([&env] () { serve(env); });
}
