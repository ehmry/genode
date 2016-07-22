/*
 * \brief  Simple loop-back pseudo network adaptor
 * \author Norman Feske
 * \date   2009-11-13
 *
 * This program showcases the server-side use of the 'Nic_session' interface.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <root/component.h>
#include <util/arg_string.h>
#include <util/misc_math.h>
#include <nic/component.h>
#include <nic/packet_allocator.h>
#include <base/component.h>

namespace Nic {

	class Loopback_component;
	class Root;
}


namespace Component { struct Main; }


class Nic::Loopback_component : public Nic::Session_component
{
	private:

		Nic::State_component           _state_rom;
		Genode::Rom_session_capability _state_cap;

	public:

		/**
		 * Constructor
		 *
		 * \param tx_buf_size        buffer size for tx channel
		 * \param rx_buf_size        buffer size for rx channel
		 * \param rx_block_md_alloc  backing store of the meta data of the
		 *                           rx block allocator
		 * \param ram_session        RAM session to allocate tx and rx buffers
		 * \param ep                 entry point used for packet stream
		 *                           channels
		 */
		Loopback_component(Genode::size_t const tx_buf_size,
		                   Genode::size_t const rx_buf_size,
		                   Genode::Allocator   &rx_block_md_alloc,
		                   Genode::Ram_session &ram,
		                   Genode::Region_map  &rm,
		                   Genode::Entrypoint  &ep)
		:
			Session_component(tx_buf_size, rx_buf_size,
			                  rx_block_md_alloc, ram, rm, ep),
			_state_rom(ram, rm), _state_cap(ep.manage(_state_rom))
		{
			Mac_address mac = {{0,2,0,0,0,1}};
			_state_rom.link_state(true);
			_state_rom.mac_addr(mac);
		}

		void _handle_packet_stream() override;

		Genode::Rom_session_capability state_rom() override {
			return _state_cap; }
};


void Nic::Loopback_component::_handle_packet_stream()
{
	using namespace Genode;

	unsigned const alloc_size = Nic::Packet_allocator::DEFAULT_PACKET_SIZE;

	/* loop unless we cannot make any progress */
	for (;;) {

		/* flush acknowledgements for the echoes packets */
		while (_rx.source()->ack_avail())
			_rx.source()->release_packet(_rx.source()->get_acked_packet());

		/*
		 * If the client cannot accept new acknowledgements for a sent packets,
		 * we won't consume the sent packet.
		 */
		if (!_tx.sink()->ready_to_ack())
			return;

		/*
		 * Nothing to be done if the client has not sent any packets.
		 */
		if (!_tx.sink()->packet_avail())
			return;

		/*
		 * Here we know that the client has submitted a packet to us and is also
		 * able it receive the corresponding acknowledgement.
		 */

		/*
		 * The client fails to pick up the packets from the rx channel. So we
		 * won't try to submit new packets.
		 */
		if (!_rx.source()->ready_to_submit())
			return;

		/*
		 * We are safe to process one packet without blocking.
		 */


		Packet_descriptor packet_to_client;
		try {
				packet_to_client = _rx.source()->alloc_packet(alloc_size);
		} catch (Session::Rx::Source::Packet_alloc_failed) {
			continue;
		}

		/* obtain packet */
		Packet_descriptor const packet_from_client = _tx.sink()->get_packet();
		if (!packet_from_client.size()) {
			PWRN("received zero-size packet");
			_rx.source()->release_packet(packet_to_client);
			continue;
		}

		Genode::memcpy(_rx.source()->packet_content(packet_to_client),
		               _tx.sink()->packet_content(packet_from_client),
		               packet_from_client.size());

		packet_to_client = Packet_descriptor(packet_to_client.offset(),
		                                     packet_from_client.size());
		_rx.source()->submit_packet(packet_to_client);

		_tx.sink()->acknowledge_packet(packet_from_client);
	}
}


class Nic::Root : public Genode::Root_component<Loopback_component>
{
	private:

		Genode::Env       &_env;
		Genode::Allocator &_alloc;

	protected:

		Loopback_component*_create_session(const char *args)
		{
			using namespace Genode;

			size_t ram_quota   = Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
			size_t rx_buf_size = Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

			/* deplete ram quota by the memory needed for the session structure */
			size_t session_size = max(4096UL, (unsigned long)sizeof(Session_component));
			if (ram_quota < session_size)
				throw Root::Quota_exceeded();

			/*
			 * Check if donated ram quota suffices for both communication
			 * buffers and check for overflow
			 */
			if (tx_buf_size + rx_buf_size < tx_buf_size ||
			    tx_buf_size + rx_buf_size > ram_quota - session_size) {
				PERR("insufficient 'ram_quota', got %zd, need %zd",
				     ram_quota, tx_buf_size + rx_buf_size + session_size);
				throw Root::Quota_exceeded();
			}

			return new (md_alloc()) Loopback_component(tx_buf_size, rx_buf_size, _alloc,
			                                          _env.ram(), _env.rm(), _env.ep());
		}

	public:

		Root(Genode::Env &env, Genode::Allocator &alloc)
		:
			Genode::Root_component<Loopback_component>(env.ep(), alloc),
			_env(env), _alloc(alloc)
		{ }
};


struct Component::Main
{
	Genode::Heap heap;

	Nic::Root nic_root;

	Main(Genode::Env &env)
	: heap(env.ram(), env.rm()), nic_root(env, heap)
	{
		env.parent().announce(env.ep().manage(nic_root));
	}
};



Genode::size_t Component::stack_size() { return 2*1024*sizeof(long); }

void Component::construct(Genode::Env &env) { static Main main(env); }

