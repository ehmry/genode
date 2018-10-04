/*
 * \brief  Native implementaion of Solo5 middleware
 * \author Emery Hemingway
 * \date   2018-08-25
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <block_session/connection.h>
#include <nic/packet_allocator.h>
#include <nic_session/connection.h>
#include <rtc_session/connection.h>
#include <timer_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/component.h>
#include <base/sleep.h>

extern "C" {
#include "bindings.h"
}


namespace Solo5 {
	using namespace Genode;
	struct Platform;
};


struct Solo5::Platform
{
	Genode::Env &env;

	Heap heap { env.pd(), env.rm() };

	Timer::Connection timer { env, "solo5" };

	Timer::One_shot_timeout<Platform> yield_timeout {
		timer, *this, &Platform::handle_timeout };

	/* TODO: periodic RTC synchronization */
	Genode::uint64_t _initial_epoch = 0;

	enum { NIC_BUFFER_SIZE =
		Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

	Genode::Allocator_avl _pkt_alloc { &heap };

	Constructible<Nic::Connection> nic { };

	Constructible<Block::Connection> block { };

	Block::sector_t blk_count = 0;
	Genode::size_t blk_size = 0;

	Io_signal_handler<Platform> nic_handler {
		env.ep(), *this, &Platform::handle_nic };

	bool nic_ready = false;

	void handle_timeout(Duration) { }

	void handle_nic() { nic_ready = true; }

	static int const *year_info(int year)
	{
		static const int standard_year[] =
			{ 365, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

		static const int leap_year[] =
			{ 366, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

		return ((year%4) == 0 && ((year%100) != 0 || (year%400) == 0))
			? leap_year : standard_year;
	}

	static Genode::uint64_t rtc_epoch(Genode::Env &env)
	{
		enum {
			SEC_PER_MIN  = 60,
			SEC_PER_HOUR = SEC_PER_MIN * 60,
			SEC_PER_DAY  = SEC_PER_HOUR * 24
		};

		auto ts = Rtc::Connection(env).current_time();

		Genode::uint64_t epoch = ts.second;
		epoch += ts.minute * SEC_PER_MIN;
		epoch += ts.hour   * SEC_PER_HOUR;

		/* add seconds from the start of the month */
		epoch += (ts.day-1) * SEC_PER_DAY;

		/* add seconds from the start of the year */
		int const *month_lengths = year_info(ts.year);
		for (unsigned m = 1; m < ts.month; ++m)
			epoch += month_lengths[m] * SEC_PER_DAY;

		/* add seconds from 1970 to the start of this year */
		for (unsigned y = 1970; y < ts.year; ++y)
			epoch += year_info(y)[0] * SEC_PER_DAY;

		return epoch;
	}

	typedef Genode::String<1024> Cmdline;
	Cmdline cmdline { };

	Platform(Genode::Env &e) : env(e)
	{
		Attached_rom_dataspace config_rom(env, "config");
		Xml_node const config = config_rom.xml();

		if (config.has_sub_node("rtc"))
			_initial_epoch = rtc_epoch(env);

		/*
		 * create sessions early to subtract session
		 * buffers from quota before the application
		 * heap is created
		 */

		if (config.has_sub_node("nic")) {
			nic.construct(
				env, &_pkt_alloc,
				NIC_BUFFER_SIZE, NIC_BUFFER_SIZE,
				"guest");
			nic->rx_channel()->sigh_packet_avail(nic_handler);
		}

		if (config.has_sub_node("blk")) {
			block.construct(env, &_pkt_alloc, 128*1024, "guest");
		}

		try {
			config.sub_node("solo5")
				.attribute("cmdline")
					.value(&cmdline);
		}
		catch (...) { }
	}

	void net_info(struct solo5_net_info &info)
	{
		if (!nic.constructed()) {
			Genode::error("network device not available");
			return;
		}

		Net::Mac_address nic_mac = nic->mac_address();

		Genode::memcpy(
			info.mac_address, nic_mac.addr,
			min(sizeof(info.mac_address), sizeof(nic_mac.addr)));
		info.mtu = 1500;
	}

	solo5_result_t net_write(const uint8_t *buf, size_t size)
	{
		if (!nic.constructed())
			return SOLO5_R_EUNSPEC;

		auto &tx = *nic->tx();

		/* free packets processed by service */
		while (tx.ack_avail())
			tx.release_packet(tx.get_acked_packet());

		if (!tx.ready_to_submit())
			return SOLO5_R_AGAIN;

		auto pkt = tx.alloc_packet(size);
		Genode::memcpy(tx.packet_content(pkt), buf, size);
		tx.submit_packet(pkt);
		return SOLO5_R_OK;
	}

	solo5_result_t net_read(uint8_t *buf, size_t size, size_t *read_size)
	{
		if (!nic.constructed())
			return SOLO5_R_EUNSPEC;

		auto &rx = *nic->rx();
		if (!rx.packet_avail() || !rx.ready_to_ack())
			return SOLO5_R_AGAIN;

		auto pkt = rx.get_packet();
		size_t n = min(size, pkt.size());
		Genode::memcpy(buf, rx.packet_content(pkt), n);
		*read_size = n;
		rx.acknowledge_packet(pkt);
		nic_ready = rx.packet_avail();
		return SOLO5_R_OK;
	}

	void block_info(struct solo5_block_info &info)
	{
		if (!block.constructed()) {
			Genode::error("block device not available");
			return;
		}

		Block::Session::Operations ops;
		block->info(&blk_count, &blk_size, &ops);

		info.capacity = blk_count * blk_size;
		info.block_size = blk_size;
	}

	solo5_result_t block_write(solo5_off_t offset,
	                           const uint8_t *buf,
	                           size_t size)
	{
		if ((offset|size) % blk_size)
			return SOLO5_R_EINVAL;
		if (!block.constructed())
			return SOLO5_R_EUNSPEC;

		auto &source = *block->tx();
		Block::Packet_descriptor pkt(
			source.alloc_packet(size),
			Block::Packet_descriptor::WRITE,
			offset / blk_size, size / blk_size);

		Genode::memcpy(source.packet_content(pkt), buf, pkt.size());
		source.submit_packet(pkt);
		pkt = source.get_acked_packet();
		source.release_packet(pkt);

		return pkt.succeeded() ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
	}

	solo5_result_t block_read(solo5_off_t offset, uint8_t *buf, size_t size)
	{
		if ((offset|size) % blk_size)
			return SOLO5_R_EINVAL;
		if (!block.constructed())
			return SOLO5_R_EUNSPEC;

		auto &source = *block->tx();
		Block::Packet_descriptor pkt(
			source.alloc_packet(size),
			Block::Packet_descriptor::READ,
			offset / blk_size, size / blk_size);

		source.submit_packet(pkt);
		pkt = source.get_acked_packet();
		Genode::memcpy(buf, source.packet_content(pkt), pkt.size());
		source.release_packet(pkt);

		return pkt.succeeded() ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
	}

	void exit(int status, void *cookie) __attribute__((noreturn))
	{
		if (cookie)
			log((Hex)addr_t(cookie));
		env.parent().exit(status);
		Genode::sleep_forever();
	}
};


static Solo5::Platform *_platform;


extern "C" {


void solo5_exit(int status)
{
    _platform->exit(status, NULL);
}


void solo5_abort(void)
{
    _platform->exit(SOLO5_EXIT_ABORT, NULL);
}


solo5_time_t solo5_clock_monotonic(void)
{
	return _platform->timer.curr_time()
		.trunc_to_plain_us().value*1000;
}


solo5_time_t solo5_clock_wall(void)
{
	return _platform->_initial_epoch * 1000000000ULL
	     + _platform->timer.curr_time().trunc_to_plain_us().value * 1000ULL;
}


bool solo5_yield(solo5_time_t deadline)
{
	solo5_time_t deadline_us = deadline / 1000;
	solo5_time_t now_us = _platform->timer.curr_time()
		.trunc_to_plain_us().value;

	_platform->yield_timeout.schedule(
		Genode::Microseconds{deadline_us - now_us});

	do {
		_platform->env.ep().wait_and_dispatch_one_io_signal();
		if (_platform->nic_ready)
			_platform->yield_timeout.discard();
	} while (_platform->yield_timeout.scheduled());

	return _platform->nic_ready;
}


void solo5_console_write(const char *buf, size_t size)
{
	while (buf[size-1] == '\0' || buf[size-1] == '\n')
		--size;

	Genode::log(Genode::Cstring(buf, size));
}


void solo5_net_info(struct solo5_net_info *info)
{
	Genode::memset(info, 0, sizeof(info));
	_platform->net_info(*info);
}


solo5_result_t solo5_net_write(const uint8_t *buf, size_t size)
{
	return _platform->net_write(buf, size);
}


solo5_result_t solo5_net_read(uint8_t *buf, size_t size, size_t *read_size)
{
	return _platform->net_read(buf, size, read_size);
}


void solo5_block_info(struct solo5_block_info *info)
{
	Genode::memset(info, 0, sizeof(info));
	_platform->block_info(*info);
}


solo5_result_t solo5_block_write(solo5_off_t offset,
                                 const uint8_t *buf,
                                 size_t size)
{
	return _platform->block_write(offset, buf, size);
}

solo5_result_t solo5_block_read(solo5_off_t offset,
                                uint8_t *buf, size_t size)
{
	return _platform->block_read(offset, buf, size);
}


} // extern "C"

void Component::construct(Genode::Env &env)
{
	static Solo5::Platform inst(env);
	_platform = &inst;

	static struct solo5_start_info si;

	si.cmdline = _platform->cmdline.string();

	/* round guest heap down to nearest 2 MiB */
	si.heap_size = env.pd().avail_ram().value & (~0UL << 20);

	Genode::Dataspace_capability heap_ds =
		env.pd().alloc(si.heap_size);

	si.heap_start = env.rm().attach(heap_ds);

	typedef Genode::Hex_range<unsigned long> Hex_range;
	Genode::log(
		Hex_range(si.heap_start, si.heap_start+si.heap_size),
		" - Solo5 heap");

	solo5_exit(solo5_app_main(&si));
}
