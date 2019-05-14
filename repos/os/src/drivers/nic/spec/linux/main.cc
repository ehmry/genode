/*
 * \brief  NIC driver for Linux TUN/TAP device
 * \author Stefan Kalkowski
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \author Emery Hemingway
 * \date   2011-08-08
 *
 * Configuration option is:
 *
 * - TAP device to connect to (default is tap0)
 *
 * This can be set in the config section as follows:
 *  <config>
 *  	<nic tap="tap1"/>
 *  </config>
 */

/*
 * Copyright (C) 2011-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <nic/packet_allocator.h>
#include <nic_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/thread.h>
#include <base/log.h>

/* Linux */
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_tun.h>

namespace Driver {
	using namespace Genode;
	struct Main;
}


struct Driver::Main
{
		struct Rx_signal_thread : Genode::Thread
		{
			int                               fd;
			Genode::Signal_context_capability sigh;

			Rx_signal_thread(Genode::Env &env, int fd, Genode::Signal_context_capability sigh)
			: Genode::Thread(env, "rx_signal", 0x1000), fd(fd), sigh(sigh) { }

			void entry() override
			{
				while (true) {
					/* wait for packet arrival on fd */
					int    ret;
					fd_set rfds;

					FD_ZERO(&rfds);
					FD_SET(fd, &rfds);
					do { ret = select(fd + 1, &rfds, 0, 0, 0); } while (ret < 0);

					/* signal incoming packet */
					Genode::Signal_transmitter(sigh).submit();
				}
			}
		};

		Env  &_env;
		Heap  _heap { _env.ram(), _env.rm() };

		int _setup_tap_fd()
		{
			Genode::Attached_rom_dataspace config_rom(_env, "config");

			/* open TAP device */
			int ret;
			struct ifreq ifr;

			int fd = open("/dev/net/tun", O_RDWR);
			if (fd < 0) {
				Genode::error("could not open /dev/net/tun: no virtual network emulation");
				/* this error is fatal */
				throw Genode::Exception();
			}

			/* set fd to non-blocking */
			if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
				Genode::error("could not set /dev/net/tun to non-blocking");
				throw Genode::Exception();
			}

			Genode::memset(&ifr, 0, sizeof(ifr));
			ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

			/* get tap device from config */
			try {
				Genode::Xml_node nic_node = config_rom.xml().sub_node("nic");
				nic_node.attribute("tap").value(ifr.ifr_name, sizeof(ifr.ifr_name));
				Genode::log("using tap device \"", Genode::Cstring(ifr.ifr_name), "\"");
			} catch (...) {
				/* use tap0 if no config has been provided */
				Genode::strncpy(ifr.ifr_name, "tap0", sizeof(ifr.ifr_name));
				Genode::log("no config provided, using tap0");
			}

			ret = ioctl(fd, TUNSETIFF, (void *) &ifr);
			if (ret != 0) {
				Genode::error("could not configure /dev/net/tun: no virtual network emulation");
				close(fd);
				/* this error is fatal */
				throw Genode::Exception();
			}

			return fd;
		}

		int             const _tap_fd { _setup_tap_fd() };
		Nic::Packet_allocator _nic_tx_alloc { &_heap };

		Nic::Connection _nic {
			_env, _nic_tx_alloc,
			Nic::Connection::default_tx_size(),
			Nic::Connection::default_rx_size(),
			"linux" };

		Io_signal_handler<Main> _link_handler {
			_env.ep(), *this, &Main::_handle_link };

		Io_signal_handler<Main> _packet_handler {
			_env.ep(), *this, &Main::_process_packets };

		Rx_signal_thread _rx_thread {
			_env, _tap_fd, _packet_handler };

		Nic::Session::Link_state _link_state { Nic::Session::LINK_UP };

		bool _send()
		{
			auto &sink = *_nic.rx();

			if (_link_state == Nic::Session::LINK_DOWN
			 || !sink.ready_to_ack()
			 || !sink.packet_avail())
				return false;

			Packet_descriptor packet = sink.get_packet();
			if (!packet.size() || !sink.packet_valid(packet)) {
				warning("invalid rx packet");
				return true;
			}

			int ret;

			/* non-blocking-write packet to TAP */
			do {
				ret = write(_tap_fd, sink.packet_content(packet), packet.size());
				/* drop packet if write would block */
				if (ret < 0 && errno == EAGAIN)
					continue;

				if (ret < 0) Genode::error("write: errno=", errno);
			} while (ret < 0);

			sink.acknowledge_packet(packet);

			return true;
		}

		bool _receive()
		{
			auto &source = *_nic.tx();

			unsigned const max_size = Nic::Packet_allocator::DEFAULT_PACKET_SIZE;

			if (_link_state == Nic::Session::LINK_DOWN
			 || !source.ready_to_submit())
				return false;

			Nic::Packet_descriptor p;
			try {
				p = source.alloc_packet(max_size);
			} catch (...) { return false; }

			int size = read(_tap_fd, source.packet_content(p), max_size);
			if (size <= 0) {
				source.release_packet(p);
				return false;
			}

			/* adjust packet size */
			Nic::Packet_descriptor p_adjust(p.offset(), size);
			source.submit_packet(p_adjust);

			return true;
		}

		void _handle_link() {
			_link_state = _nic.session_link_state(); }

		void _process_packets()
		{
			auto &source = *_nic.tx();
			while (source.ack_avail())
				source.release_packet(source.get_acked_packet());

			while (_send()) ;
			while (_receive()) ;
		}

		Main(Genode::Env &env) : _env(env)
		{
			_rx_thread.start();

			/* set Nic session signal handlers */
			_nic.rx_channel()->sigh_packet_avail(_packet_handler);
			_nic.rx_channel()->sigh_ready_to_ack(_packet_handler);
			_nic.link_state_sigh(_link_handler);

			_nic.link_state( Nic::Session::LINK_UP);
		}
};

void Component::construct(Genode::Env &env) { static Driver::Main main(env); }
