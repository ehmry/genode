/*
 * \brief  HTTP client test
 * \author Ivan Loskutov
 * \date   2012-12-21
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <libc/component.h>
#include <nic/packet_allocator.h>
#include <timer_session/connection.h>
#include <util/string.h>

extern "C" {
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
}


/**
 * The client simply loops endless,
 * and sends as much 'http get' requests as possible,
 * printing out the response.
 */
void http_clnt(Libc::Env &env)
{
	using namespace Genode;
	using Address = Genode::String<16>;

	enum { BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

	static Timer::Connection _timer(env);
	_timer.msleep(2000);

	Address serv_addr;

	Attached_rom_dataspace config(env, "config");
	Xml_node config_node = config.xml();
	config_node.attribute("server_ip").value(&serv_addr);

	for(int j = 0; j != 5; ++j) {
		_timer.msleep(2000);

		log("Create new socket ...");
		int s = socket(AF_INET, SOCK_STREAM, 0 );
		if (s < 0) {
			error("no socket available!");
			continue;
		}

		log("Connect to server ...");

		unsigned port = 0;
		try { config_node.attribute("server_port").value(&port); }
		catch (...) {
			error("Missing \"server_port\" attribute.");
			throw Xml_node::Nonexistent_attribute();
		}

		struct sockaddr_in addr;
		addr.sin_port = htons(port);
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(serv_addr.string());

		if((connect(s, (struct sockaddr *)&addr, sizeof(addr))) < 0) {
			error("Could not connect!");
			close(s);
			continue;
		}

		log("Send request...");

		/* simple HTTP request header */
		static const char *http_get_request =
			"GET / HTTP/1.0\r\nHost: localhost:80\r\n\r\n";

		unsigned long bytes = send(s, (char*)http_get_request,
		                                Genode::strlen(http_get_request), 0);
		if ( bytes < 0 ) {
			error("couldn't send request ...");
			close(s);
			continue;
		}

		/* Receive http header and content independently in 2 packets */
		for(int i=0; i<2; i++) {
			char buf[1024];
			ssize_t buflen;
			buflen = recv(s, buf, 1024, 0);
			if(buflen > 0) {
				buf[buflen] = 0;
				log("Received \"", String<64>(buf), " ...\"");
			} else
				break;
		}

		/* Close socket */
		close(s);
	}

	log("Test done");
}

void Libc::Component::construct(Libc::Env &env) {
	with_libc([&env] () { http_clnt(env); }); }
