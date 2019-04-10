/*
 * \brief  Packet handler handling network packets.
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <net/arp.h>
#include <net/dhcp.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <net/ipv6.h>
#include <net/udp.h>
#include <net/icmp6.h>

#include <component.h>
#include <packet_handler.h>

using namespace Net;

void Packet_handler::_ready_to_submit()
{
	/* as long as packets are available, and we can ack them */
	while (sink()->packet_avail()) {
		_packet = sink()->get_packet();
		if (!_packet.size() || !sink()->packet_valid(_packet)) continue;
		handle_ethernet(sink()->packet_content(_packet), _packet.size());

		if (!sink()->ready_to_ack()) {
			Genode::warning("ack state FULL");
			return;
		}

		sink()->acknowledge_packet(_packet);
	}
}


void Packet_handler::_ready_to_ack()
{
	/* check for acknowledgements */
	while (source()->ack_avail())
		source()->release_packet(source()->get_acked_packet());
}


void Packet_handler::_link_state()
{
	Mac_address_node *node = _vlan.mac_list.first();
	while (node) {
		node->component().link_state_changed();
		node = node->next();
	}
}


void Packet_handler::broadcast_to_clients(Ethernet_frame *eth, Genode::size_t size)
{
	/* iterate through the list of clients */
	Mac_address_node *node =
		_vlan.mac_list.first();
	while (node) {
		/* deliver packet */
		if (node->component().vmac() != eth->src())
			node->component().send(eth, size);
		node = node->next();
	}
}


void Packet_handler::handle_ethernet(void* src, Genode::size_t size)
{
	try {
		bool broadcast = false;

		/* parse ethernet frame header */
		Size_guard size_guard(size);
		Ethernet_frame &eth = Ethernet_frame::cast_from(src, size_guard);
		if (_verbose) {
			Genode::log("[", _label, "] rcv ", eth); }
		broadcast = eth.dst() == Ethernet_frame::broadcast();
		switch (eth.type()) {

		case Ethernet_frame::Type::ARP:
			if (!handle_arp(eth, size_guard)) return;
			break;

		case Ethernet_frame::Type::IPV4:
			if (!handle_ip4(eth, size_guard)) return;
			break;

		case Ethernet_frame::Type::IPV6: {
			auto &ip = eth.data<Ipv6_packet>(size_guard);
			broadcast |= (ip.dst().multicast());
			if (!handle_ip6(eth, ip, size_guard)) return;
			break;
		}

		default:
			;
		}

		if (broadcast)
			broadcast_to_clients(&eth, size);
		finalize_packet(&eth, size);
	} catch(Size_guard::Exceeded) {
		Genode::warning("Packet size guard exceeded!");
	}
}


bool Packet_handler::handle_ip6(Ethernet_frame &eth,
                                Ipv6_packet    &ip,
                                Size_guard     &size_guard)
{
	if (ip.protocol() == Ip_protocol::ICMP6) {

		auto &icmp = ip.data<Icmp6_packet>(size_guard);

		auto const hit = [] (Neighbor_entry &) { };
		auto const miss_src = [&] () {
			_neighbors.new_entry(ip.src(), eth.src(), Neighbor_entry::STALE); };
		auto const miss_dst = [&] () {
			_neighbors.new_entry(ip.dst(), eth.dst(), Neighbor_entry::STALE); };

		switch (icmp.type()) {
			case Icmp6_packet::Type::NEIGHBOR_SOLICITATION:
				if (!ip.src().unspecified()) {
					_neighbors.apply(ip.src(), hit, miss_src);
				}
				if (!ip.dst().unspecified()) {
					_neighbors.apply(ip.dst(), hit, miss_dst);
				}
				break;

			case Icmp6_packet::Type::NEIGHBOR_ADVERTISEMENT:
				if (!ip.src().unspecified()) {
					auto const hit = [&] (Neighbor_entry &ne) {
						ne.mac = eth.src();
						ne.state = Neighbor_entry::REACHABLE;
					};
					auto const miss = [&] () {
						_neighbors.new_entry(ip.src(), eth.src(),
						                     Neighbor_entry::REACHABLE);
					};
					_neighbors.apply(ip.src(), hit, miss);
				}
				Genode::error("NA unhandled!"); break;

			case Icmp6_packet::Type::ROUTER_SOLICITATION:
				Genode::error("RS unhandled!"); break;

			case Icmp6_packet::Type::ROUTER_ADVERTISEMENT: {
				auto &ra = icmp.data<Router_advertisement>(size_guard);
				if (ra.proxied()) {
					Genode::error("proxy loop detected at ", _label);
					return false;
				}
				ra.proxied(true);
				Genode::error("RA unhandled!"); break;
			}

			case Icmp6_packet::Type::REDIRECT:
				Genode::error("redirect unhandled!"); break;
		}
	}

	return true;
}


void Packet_handler::send(Ethernet_frame *eth, Genode::size_t size)
{
	if (_verbose || true) {
		Genode::log("[", _label, "] snd ", *eth); }
	try {
		/* copy and submit packet */
		Packet_descriptor packet  = source()->alloc_packet(size);
		char             *content = source()->packet_content(packet);
		Genode::memcpy((void*)content, (void*)eth, size);
		source()->submit_packet(packet);
	} catch(Packet_stream_source< ::Nic::Session::Policy>::Packet_alloc_failed) {
		Genode::warning("Packet dropped");
	}
}


Packet_handler::Packet_handler(Genode::Entrypoint          &ep,
                               Vlan                        &vlan,
                               Genode::Session_label const &label,
                               bool                  const &verbose)
:
  _vlan(vlan),
  _label(label),
  _verbose(verbose),
  _sink_ack(ep, *this, &Packet_handler::_ack_avail),
  _sink_submit(ep, *this, &Packet_handler::_ready_to_submit),
  _source_ack(ep, *this, &Packet_handler::_ready_to_ack),
  _source_submit(ep, *this, &Packet_handler::_packet_avail),
  _client_link_state(ep, *this, &Packet_handler::_link_state)
{
	if (_verbose) {
		Genode::log("[", _label, "] interface initialized"); }
}
