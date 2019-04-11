/*
 * \brief  Internet protocol version 6
 * \author Emery Hemingway
 * \date   2019-04-07
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <net/udp.h>
#include <net/tcp.h>
#include <net/icmp6.h>
#include <net/ipv6.h>

using namespace Genode;
using namespace Net;

enum { ADDR_BYTES = 16 };

bool Net::Ipv6_address::unspecified() const
{
	uint8_t accum = 0;
	for (auto i = 0U; i < ADDR_BYTES; ++i) accum |= addr[i];
	return !accum;
}


bool Net::Ipv6_address::loopback() const
{
	for (auto i = 0U; i < ADDR_BYTES-1; ++i)
		if (addr[i]) return false;
	return addr[ADDR_BYTES-1] == 1;
}


bool Net::Ipv6_address::multicast() const
{
	return addr[0] == 0xff;
}


bool Net::Ipv6_address::link_local_unicast() const
{
	return (addr[0] == 0xfe) && ((addr[1]>>6) == 0x02);
}


void Net::Ipv6_address::print(Genode::Output &output) const
{
	enum { FIELD_COUNT = 8 };
	uint16_t fields[FIELD_COUNT];

	for (auto i = 0U; i < FIELD_COUNT; ++i) {
		fields[i] =
			  addr[i<<1|0] << 8
			| addr[i<<1|1] << 0;
	}

	int collapse_offset = -1;
	int collapse_count  = 0;

	for (auto i = 0U; i < FIELD_COUNT; ++i) {
		int count = 0;
		for (int j = i; j < FIELD_COUNT; ++j) {
			if (fields[j]) break;
			++count;
		}
		if (count && collapse_count < count) {
			collapse_offset = i;
			collapse_count = count;
		}
	}

	if (collapse_count == 1) collapse_offset = -1;
	(void)collapse_offset;

	for (int i = 0; i < FIELD_COUNT; ++i) {
		if (collapse_offset == i) {
			output.out_char(':');
			i += collapse_count;
			if (FIELD_COUNT <= i) {
				output.out_char(':');
				break;
			}
		}

		Genode::Hex(fields[i], Hex::OMIT_PREFIX, Hex::NO_PAD).print(output);
		if (i < FIELD_COUNT-1)
			output.out_char(':');
	}
}


size_t Ipv6_packet::size(size_t max_size) const
{
	size_t const stated_size = total_length();
	return stated_size < max_size ? stated_size : max_size;
}


size_t Ipv6_packet::total_length() const
{
	enum { IPV6_HEADER_LEN = 40 };
	return IPV6_HEADER_LEN+host_to_big_endian(_payload_length);
	/* extension headers are included in the payload length field */
}


void Net::Ipv6_packet::print(Genode::Output &output) const
{
	Genode::print(output, "\033[32mIPv6\033[0m ", src(), " > ", dst(), " ");
	switch (protocol()) {
	case Ip_protocol::TCP:
		Genode::print(output, *reinterpret_cast<Tcp_packet const *>(_data));
		break;
	case Ip_protocol::UDP:
		Genode::print(output, *reinterpret_cast<Udp_packet const *>(_data));
		break;
	case Ip_protocol::ICMP6:
		Genode::print(output, *reinterpret_cast<Icmp6_packet const *>(_data));
		break;
	default: ; }
}
