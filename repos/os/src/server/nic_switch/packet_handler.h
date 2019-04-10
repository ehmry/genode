/*
 * \brief  Signal driven NIC packet handler
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PACKET_HANDLER_H_
#define _PACKET_HANDLER_H_

/* Genode */
#include <base/semaphore.h>
#include <base/thread.h>
#include <nic_session/connection.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <base/log.h>
#include <net/arp.h>
#include <net/dhcp.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <net/ipv6.h>
#include <net/udp.h>
#include <net/icmp6.h>

#include "component.h"
#include "vlan.h"

namespace Nic_switch {
	using namespace Net;

	class Packet_handler;

	using ::Nic::Packet_stream_sink;
	using ::Nic::Packet_stream_source;
	typedef ::Nic::Packet_descriptor Packet_descriptor;
}

/**
 * Generic packet handler used as base for NIC and client packet handlers.
 */
class Nic_switch::Packet_handler
{
	private:

		Packet_descriptor      _packet { };
		Nic_switch::Vlan      &_vlan;
		Genode::Session_label  _label;
		bool            const &_verbose;

		/**
		 * submit queue not empty anymore
		 */
		void _ready_to_submit();

		/**
		 * acknoledgement queue not full anymore
		 *
		 * TODO: by now, we assume ACK and SUBMIT queue to be equally
		 *       dimensioned. That's why we ignore this signal by now.
		 */
		void _ack_avail() { }

		/**
		 * acknoledgement queue not empty anymore
		 */
		void _ready_to_ack();

		/**
		 * submit queue not full anymore
		 *
		 * TODO: by now, we just drop packets that cannot be transferred
		 *       to the other side, that's why we ignore this signal.
		 */
		void _packet_avail() { }

		/**
		 * the link-state of changed
		 */
		void _link_state();

	protected:

		Genode::Signal_handler<Packet_handler> _sink_ack;
		Genode::Signal_handler<Packet_handler> _sink_submit;
		Genode::Signal_handler<Packet_handler> _source_ack;
		Genode::Signal_handler<Packet_handler> _source_submit;
		Genode::Signal_handler<Packet_handler> _client_link_state;

	public:

		Packet_handler(Genode::Entrypoint&,
		               Vlan&,
		               Genode::Session_label const &label,
		               bool                  const &verbose);

		virtual ~Packet_handler() { }

		virtual Packet_stream_sink< ::Nic::Session::Policy>   * sink()   = 0;
		virtual Packet_stream_source< ::Nic::Session::Policy> * source() = 0;

		Nic_switch::Vlan & vlan() { return _vlan; }

		/**
		 * Broadcasts ethernet frame to all clients,
		 * as long as its really a broadcast packtet.
		 *
		 * \param eth   ethernet frame to send.
		 * \param size  ethernet frame's size.
		 */
		void broadcast_to_clients(Ethernet_frame *eth,
		                          Genode::size_t size);

		/**
		 * Send ethernet frame
		 *
		 * \param eth   ethernet frame to send.
		 * \param size  ethernet frame's size.
		 */
		void send(Ethernet_frame *eth, Genode::size_t size);

		/**
		 * Handle an ethernet packet
		 *
		 * \param src   ethernet frame's address
		 * \param size  ethernet frame's size.
		 */
		void handle_ethernet(void* src, Genode::size_t size);

		/*
		 * Handle an ARP packet
		 *
		 * \param eth   ethernet frame containing the ARP packet.
		 * \param size  ethernet frame's size.
		 */
		virtual bool handle_arp(Ethernet_frame &eth,
		                        Size_guard     &size_guard)   = 0;

		/*
		 * Handle an IPv4 packet
		 *
		 * \param eth   ethernet frame containing the IPv4 packet.
		 * \param size  ethernet frame's size.
		 */
		virtual bool handle_ip4(Ethernet_frame &eth,
		                        Size_guard     &size_guard) = 0;

		/*
		 * Handle an IPv6 packet
		 *
		 * \param eth   ethernet frame containing the IPv6 packet.
		 * \param size  ethernet frame's size.
		 */
		bool handle_ip6(Ethernet_frame &eth,
		                Ipv6_packet    &ip,
		                Size_guard     &size_guard);

		/*
		 * Finalize handling of ethernet frame.
		 *
		 * \param eth   ethernet frame to handle.
		 * \param size  ethernet frame's size.
		 */
		virtual void finalize_packet(Ethernet_frame *eth,
		                             Genode::size_t size) = 0;
};

void Nic_switch::Packet_handler::_ready_to_submit()
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


void Nic_switch::Packet_handler::_ready_to_ack()
{
	/* check for acknowledgements */
	while (source()->ack_avail())
		source()->release_packet(source()->get_acked_packet());
}


void Nic_switch::Packet_handler::handle_ethernet(void* src, Genode::size_t size)
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


bool Nic_switch::Packet_handler::handle_ip6(Ethernet_frame &,
                                Ipv6_packet    &ip,
                                Size_guard     &size_guard)
{
	if (ip.protocol() == Ip_protocol::ICMP6) {

		auto &icmp = ip.data<Icmp6_packet>(size_guard);

		switch (icmp.type()) {
			case Icmp6_packet::Type::NEIGHBOR_SOLICITATION: break;

			case Icmp6_packet::Type::NEIGHBOR_ADVERTISEMENT: break;

			case Icmp6_packet::Type::ROUTER_SOLICITATION: break;

			case Icmp6_packet::Type::ROUTER_ADVERTISEMENT: {
				auto &ra = icmp.data<Router_advertisement>(size_guard);
				if (ra.proxied()) {
					Genode::error("proxy loop detected at ", _label);
					return false;
				}
				ra.proxied(true);
				break;
			}

			case Icmp6_packet::Type::REDIRECT: break;
		}
	}

	return true;
}


void Nic_switch::Packet_handler::send(Ethernet_frame *eth, Genode::size_t size)
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


Nic_switch::Packet_handler::Packet_handler(Genode::Entrypoint          &ep,
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

#endif
