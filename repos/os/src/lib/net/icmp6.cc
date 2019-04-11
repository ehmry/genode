/*
 * \brief  Internet Control Message Protocol for IPv6
 * \author Emery Hemingway
 * \date   2018-04-07
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <net/icmp6.h>

using namespace Net;
using namespace Genode;


void Net::Icmp6_packet::print(Output &output) const
{
	Genode::print(output,
		error() ? "\033[31m" : "\033[32m", "ICMPv6\033[0m ");

	switch (type()) {
	case Net::Icmp6_packet::Type::ROUTER_SOLICITATION:
		output.out_string("ND-RS"); break;
	case Net::Icmp6_packet::Type::ROUTER_ADVERTISEMENT:
		output.out_string("ND-RA"); break;
	case Net::Icmp6_packet::Type::NEIGHBOR_SOLICITATION:
		output.out_string("ND-NS"); break;
	case Net::Icmp6_packet::Type::NEIGHBOR_ADVERTISEMENT:
		output.out_string("ND-NA"); break;
	default:
		Genode::print(output, (int)type(), " ", (int)code()); break;
	}
}
