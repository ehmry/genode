/*
 * \brief  Solo5 hardware virtualization tender
 * \author Emery Hemingway
 * \date   2018-09-16
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __HVT__DEVICES_H_
#define __HVT__DEVICES_H_

/* Genode includes */
#include <block_session/connection.h>
#include <nic/packet_allocator.h>
#include <nic_session/connection.h>
#include <rtc_session/connection.h>
#include <timer_session/connection.h>
#include <log_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/component.h>
#include <base/sleep.h>

namespace Hvt {
	using namespace Genode;
	struct Devices;
	using Genode::uint64_t;

	struct Invalid_guest_access : Genode::Exception { };

	template<typename T>
	T &gpa_cast(struct hvt *hvt, hvt_gpa_t gpa)
	{
	    hvt_gpa_t r;

	    if ((gpa >= hvt->mem_size) ||
		    add_overflow(gpa, sizeof(T), r) ||
		    (r >= hvt->mem_size))
		{
			Genode::error("Invalid guest access: gpa=", gpa, ", sz=", sizeof(T));
			throw Invalid_guest_access();
    	}

		return *static_cast<T *>(
			static_cast<void*>(hvt->mem + gpa));
	}

	template<typename T>
	T *gpa_buffer_cast(struct hvt *hvt, hvt_gpa_t gpa, size_t len)
	{
	    hvt_gpa_t r;

	    if ((gpa >= hvt->mem_size) ||
		    add_overflow(gpa, len, r) ||
		    (r >= hvt->mem_size))
		{
			Genode::error("Invalid guest access: gpa=", gpa, ", sz=", len);
			throw Invalid_guest_access();
    	}

		return static_cast<T *>(
			static_cast<void*>(hvt->mem + gpa));
	}
}

struct Hvt::Devices
{
	Genode::Env &_env;

	Timer::Connection _timer { _env };

	Timer::One_shot_timeout<Devices> _poll_timeout {
		_timer, *this, &Devices::_handle_timeout };

	/* TODO: periodic timeout to sychronize with the RTC */

	uint64_t _initial_epoch = 0;

	Log_connection _log { _env, "guest" };

	Heap _heap { _env.pd(), _env.rm() };

	enum { NIC_BUFFER_SIZE =
		Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

	Genode::Allocator_avl _pkt_alloc { &_heap };

	Constructible<Nic::Connection> _nic { };

	struct hvt_netinfo _nic_info { };

	Constructible<Block::Connection> _block { };

	struct hvt_blkinfo _block_info { };

	Io_signal_handler<Devices> _nic_handler {
		_env.ep(), *this, &Devices::_handle_nic };

	bool _nic_ready = false;

	void _handle_timeout(Duration) { }

	void _handle_nic() {_nic_ready = true; }

	void _walltime(struct hvt *hvt, hvt_gpa_t gpa)
	{
		struct hvt_walltime &time = gpa_cast<struct hvt_walltime>(hvt, gpa);
		time.nsecs = _initial_epoch + _timer.curr_time().trunc_to_plain_ms().value / 1000;
	}

	void _puts(struct hvt *hvt, hvt_gpa_t gpa)
	{
		struct hvt_puts &puts = gpa_cast<struct hvt_puts>(hvt, gpa);
		char const *buf = gpa_buffer_cast<char>(hvt, puts.data, puts.len);
		_log.write(Log_session::String(buf, puts.len));
	}

	void _poll(struct hvt *hvt, hvt_gpa_t gpa)
	{
		struct hvt_poll &poll = gpa_cast<struct hvt_poll>(hvt, gpa);

		if (_nic_ready) {
			poll.ret = 1;
			return;
		}

		_poll_timeout.schedule(Microseconds{poll.timeout_nsecs * 1000});

		do {
			_env.ep().wait_and_dispatch_one_io_signal();
			if (_nic_ready)
				_poll_timeout.discard();
		} while (_poll_timeout.scheduled());

		poll.ret = _nic_ready;
	}

	void _blkinfo(struct hvt *hvt, hvt_gpa_t gpa)
	{
		struct hvt_blkinfo &info = gpa_cast<struct hvt_blkinfo>(hvt, gpa);

		info.sector_size = _block_info.sector_size;
		info.num_sectors = _block_info.num_sectors;
		info.rw = _block_info.rw;
	}

	void _blkwrite(struct hvt *hvt, hvt_gpa_t gpa)
	{
		struct hvt_blkwrite &wr = gpa_cast<struct hvt_blkwrite>(hvt, gpa);
		off_t pos, end;

		if (wr.sector >= _block_info.num_sectors) {
			wr.ret = -1;
			return;
		}
		pos = (off_t)_block_info.sector_size * (off_t)wr.sector;
		if (add_overflow(pos, wr.len, end) ||
			(end > off_t(_block_info.num_sectors * _block_info.sector_size)))
		{
			wr.ret = -1;
			return;
		}

		auto &source = *_block->tx();

		Block::Packet_descriptor pkt(
			source.alloc_packet(wr.len),
			Block::Packet_descriptor::WRITE,
			wr.sector / _block_info.sector_size,
			wr.len / _block_info.sector_size);

		Genode::memcpy(
			source.packet_content(pkt),
			gpa_buffer_cast<char>(hvt, wr.data, wr.len),
			wr.len);

		source.submit_packet(pkt);
		pkt = source.get_acked_packet();
		source.release_packet(pkt);

		wr.ret = 0 - pkt.succeeded();
	}

	void _blkread(struct hvt *hvt, hvt_gpa_t gpa)
	{
		struct hvt_blkread &rd = gpa_cast<struct hvt_blkread>(hvt, gpa);
		off_t pos, end;

		if (rd.sector >= _block_info.num_sectors) {
			rd.ret = -1;
			return;
		}
		pos = (off_t)_block_info.sector_size * (off_t)rd.sector;
		if (add_overflow(pos, rd.len, end) ||
		    (end > off_t(_block_info.num_sectors * _block_info.sector_size)))
		{
			rd.ret = -1;
			return;
		}

		auto &source = *_block->tx();

		Block::Packet_descriptor pkt(
			source.alloc_packet(rd.len),
			Block::Packet_descriptor::READ,
			rd.sector / _block_info.sector_size,
			rd.len / _block_info.sector_size);

		source.submit_packet(pkt);
		pkt = source.get_acked_packet();
		if (!pkt.succeeded()) {
			rd.ret = -1;
			return;
		}

		Genode::memcpy(
			gpa_buffer_cast<char>(hvt, rd.data, rd.len),
			source.packet_content(pkt),
			pkt.size());
		source.release_packet(pkt);

		rd.ret = 0;
	}

	void _netinfo(struct hvt *hvt, hvt_gpa_t gpa)
	{
		struct hvt_netinfo &info = gpa_cast<struct hvt_netinfo>(hvt, gpa);

		Genode::memcpy(
			info.mac_address,
			_nic_info.mac_address,
			sizeof(_nic_info.mac_address));
	}

	void _netwrite(struct hvt *hvt, hvt_gpa_t gpa)
	{
		struct hvt_netwrite &wr = gpa_cast<struct hvt_netwrite>(hvt, gpa);
		if (!_nic.constructed()) {
			wr.ret = -1;
			return;
		}

		auto &tx = *_nic->tx();

		/* free packets processed by service */
		while (tx.ack_avail())
			tx.release_packet(tx.get_acked_packet());

		if (!tx.ready_to_submit()) {
			wr.ret = -1;
			return;
		}

		auto pkt = tx.alloc_packet(wr.len);
		Genode::memcpy(
			tx.packet_content(pkt),
			gpa_buffer_cast<char>(hvt, wr.data, wr.len), wr.len);
		tx.submit_packet(pkt);
		wr.ret = 0;
	}

	void _netread(struct hvt *hvt, hvt_gpa_t gpa)
	{
		struct hvt_netread &rd = gpa_cast<struct hvt_netread>(hvt, gpa);
		if (!_nic.constructed()) {
			rd.ret = -1;
			return;
		}

		auto &rx = *_nic->rx();
		if (!rx.packet_avail() || !rx.ready_to_ack()) {
			rd.ret = -1;
			return;
		}

		auto pkt = rx.get_packet();
		rd.len = min(rd.len, pkt.size());
		Genode::memcpy(gpa_buffer_cast<char>(hvt, rd.data, rd.len), rx.packet_content(pkt), rd.len);
		rx.acknowledge_packet(pkt);
		_nic_ready = rx.packet_avail();
		rd.ret = 0;
	}

	void _halt(struct hvt *hvt, hvt_gpa_t gpa)
	{
		struct hvt_halt &halt = gpa_cast<struct hvt_halt>(hvt, gpa);
		if (halt.cookie)
			log("Halt: ", (Hex)Genode::addr_t(halt.cookie));
		_env.parent().exit(halt.exit_status);
		sleep_forever();
	}

	static int const *year_info(int year)
	{
		static const int standard_year[] =
			{ 365, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

		static const int leap_year[] =
			{ 366, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

		return ((year%4) == 0 && ((year%100) != 0 || (year%400) == 0))
			? leap_year : standard_year;
	}

	static uint64_t rtc_epoch(Genode::Env &env)
	{
		enum {
			SEC_PER_MIN  = 60,
			SEC_PER_HOUR = SEC_PER_MIN * 60,
			SEC_PER_DAY  = SEC_PER_HOUR * 24
		};

		auto ts = Rtc::Connection(env).current_time();

		uint64_t epoch = ts.second;
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

	Devices(Genode::Env &env) : _env(env)
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
			_nic.construct(
				env, &_pkt_alloc,
				NIC_BUFFER_SIZE, NIC_BUFFER_SIZE,
				"guest");
			_nic->rx_channel()->sigh_packet_avail(_nic_handler);

			Net::Mac_address nic_mac = _nic->mac_address();

			Genode::memcpy(
				_nic_info.mac_address, nic_mac.addr,
				min(sizeof(_nic_info.mac_address), sizeof(nic_mac.addr)));
		}

		if (config.has_sub_node("blk")) {
			_block.construct(env, &_pkt_alloc, 128*1024, "guest");

			typedef Block::Packet_descriptor::Opcode Opcode;

			Block::Session::Operations ops { };
			Block::sector_t blk_count = 0;
			Genode::size_t  blk_size = 0;

			_block->info(&blk_count, &blk_size, &ops);

			_block_info.sector_size = blk_size;
			_block_info.num_sectors = blk_size;
			_block_info.rw = ops.supported(Opcode::WRITE);
		}
	}

};

#endif
