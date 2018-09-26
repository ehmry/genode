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

#include <base/debug.h>

/* Genode includes */
#include <vmm/vcpu_thread.h>
#include <block_session/connection.h>
#include <nic/packet_allocator.h>
#include <nic_session/connection.h>
#include <rtc_session/connection.h>
#include <timer_session/connection.h>
#include <log_session/connection.h>
#include <pd_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <base/heap.h>
#include <base/component.h>
#include <base/sleep.h>

/* VMM utility includes */
#include <vmm/guest_memory.h>
#include <vmm/vcpu_thread.h>
#include <vmm/vcpu_dispatcher.h>
#include <vmm/utcb_guard.h>

/* NOVA includes that come with Genode */
#include <nova/syscalls.h>
#include <nova/print.h>

extern "C" {

/* Upstream HVT includes */
#include "hvt.h"
#include "hvt_cpu_x86_64.h"

/* Libc includes */
#include <elf.h>

}

namespace Hvt {
	using namespace Genode;
	using Genode::uint64_t;

	/* used to create 2 MiB mappings */
	enum {
		GUEST_PAGE_MASK = ~(X86_GUEST_PAGE_SIZE-1),
		GUEST_PAGE_ORDER = 9
	};

	struct Devices;
	struct Guest_memory;
	struct Vcpu_handler;
	struct Main;

	struct Invalid_guest_image : Genode::Exception { };
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
};


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


struct Hvt::Guest_memory
{
	Genode::Env &_env;

	size_t _guest_quota()
	{
		size_t avail_ram = _env.ram().avail_ram().value;
		size_t avail_pages = min(size_t(512), avail_ram / size_t(X86_GUEST_PAGE_SIZE));
		if (!avail_pages) {
			error("insufficient RAM quota to host an HVT guest");
			throw Out_of_ram();
		}
		return avail_pages * X86_GUEST_PAGE_SIZE;
	}

	size_t const _guest_ram_size = _guest_quota();

	Attached_ram_dataspace _ram_ds {
		_env.pd(), _env.rm(), _guest_ram_size };

	/* elf entry point (on physical memory) */
	hvt_gpa_t _p_entry = 0;

	/* highest byte of the program (on physical memory) */
	hvt_gpa_t _p_end   = 0;

	void _load_elf(Genode::Env &env)
	{
		Attached_rom_dataspace elf_rom(env, "guest");

		uint8_t *mem = _ram_ds.local_addr<uint8_t>();
		memset(mem, 0xff, _guest_ram_size);
		Genode::log("Guest base address: ", (Hex)(addr_t)mem);


		size_t mem_size = _guest_ram_size;

		Elf64_Phdr *phdr = NULL;
		Elf64_Ehdr &hdr = *elf_rom.local_addr<Elf64_Ehdr>();

		/*
		 * Validate program is in ELF64 format:
		 * 1. EI_MAG fields 0, 1, 2, 3 spell ELFMAG('0x7f', 'E', 'L', 'F'),
		 * 2. File contains 64-bit objects,
		 * 3. Objects are Executable,
		 * 4. Target instruction must be set to the correct architecture.
		 */
		if (hdr.e_ident[EI_MAG0] != ELFMAG0
		        || hdr.e_ident[EI_MAG1] != ELFMAG1
		        || hdr.e_ident[EI_MAG2] != ELFMAG2
		        || hdr.e_ident[EI_MAG3] != ELFMAG3
		        || hdr.e_ident[EI_CLASS] != ELFCLASS64
		        || hdr.e_type != ET_EXEC
#if defined(__x86_64__)
		        || hdr.e_machine != EM_X86_64
#elif defined(__aarch64__)
		        || hdr.e_machine != EM_AARCH64
#else
#error Unsupported target
#endif
		    )
		    throw Invalid_guest_image();

		phdr = (Elf64_Phdr *)(elf_rom.local_addr<char>()+hdr.e_phoff);

		/*
		 * Load all segments with the LOAD directive from the elf file at offset
		 * p_offset, and copy that into p_addr in memory. The amount of bytes
		 * copied is p_filesz.  However, each segment should be given
		 * p_memsz aligned up to p_align bytes on memory.
		 */
		for (Elf64_Half ph_i = 0; ph_i < hdr.e_phnum; ph_i++) {
			uint8_t *daddr;
			uint64_t _end;
			size_t  offset = phdr[ph_i].p_offset;
			size_t  filesz = phdr[ph_i].p_filesz;
			size_t   memsz = phdr[ph_i].p_memsz;
			uint64_t paddr = phdr[ph_i].p_paddr;
			uint64_t align = phdr[ph_i].p_align;
			uint64_t result;
			int prot;

			if (phdr[ph_i].p_type != PT_LOAD)
			    continue;

			if ((paddr >= mem_size) || add_overflow(paddr, filesz, result)
			        || (result >= mem_size))
			    throw Invalid_guest_image();
			if (add_overflow(paddr, memsz, result) || (result >= mem_size))
			    throw Invalid_guest_image();
			/*
			 * Verify that align is a non-zero power of 2 and safely compute
			 * ((_end + (align - 1)) & -align).
			 */
			if (align > 0 && (align & (align - 1)) == 0) {
			    if (add_overflow(result, (align - 1), _end))
			        throw Invalid_guest_image();
			    _end = _end & -align;
			}
			else {
				_end = result;
			}
			if (_end > _p_end)
				_p_end = _end;

			daddr = mem + paddr;
			memcpy(daddr, elf_rom.local_addr<uint8_t>()+offset, filesz);
			log("ELF: ", (Hex)paddr, "...", Hex(paddr+filesz));

			enum {
				PROT_NONE  = 0x00U, /* no permissions */
				PROT_READ  = 0x01U, /* pages can be read */
				PROT_WRITE = 0x02U, /* pages can be written */
				PROT_EXEC  = 0x04U, /* pages can be executed */
			};

			prot = PROT_NONE;
			if (phdr[ph_i].p_flags & PF_R)
				prot |= PROT_READ;
			if (phdr[ph_i].p_flags & PF_W) {
				prot |= PROT_WRITE;
				warning("phdr[", ph_i, "] requests WRITE permissions");
			}
			if (phdr[ph_i].p_flags & PF_X) {
				prot |= PROT_EXEC;
				warning("phdr[", ph_i, "] requests EXEC permissions");
			}
		}

		_p_entry = hdr.e_entry;
		return;
	}

	Guest_memory(Genode::Env &env) : _env(env)
	{
		memset(base(), 0, size());

		log("Load guest ELF...");
		_load_elf(env);
		log("Guest ELF valid! entry=", (Hex)_p_entry, ", end=", (Hex)_p_end);

		hvt_x86_setup_gdt(base());
		hvt_x86_setup_pagetables(base(), size());

		enum { TODO_TSC_GUESS = 7 };

		struct hvt_boot_info *bi =
			(struct hvt_boot_info *)(base() + X86_BOOT_INFO_BASE);
		bi->mem_size = _guest_ram_size;
		bi->kernel_end = _p_end;
		bi->cmdline = X86_CMDLINE_BASE;
		bi->cpu.tsc_freq = TODO_TSC_GUESS;

		char *cmdline = _ram_ds.local_addr<char>()+X86_CMDLINE_BASE;
		Genode::snprintf(cmdline, HVT_CMDLINE_SIZE, "NOVA HVT");
	}

	uint8_t *base() const {
		return _ram_ds.local_addr<uint8_t>(); }

	addr_t addr() const {
		return (addr_t)_ram_ds.local_addr<uint8_t>(); }

	size_t size() const {
		return _guest_ram_size; }
};


struct Hvt::Vcpu_handler : Vmm::Vcpu_dispatcher<Genode::Thread>
{
		Guest_memory        &_guest_memory;
		Pd_session          &_guest_pd;
		Vmm::Vcpu_other_pd   _vcpu_thread;

		static Nova::Utcb &_utcb_of_myself()
		{
			return *reinterpret_cast<Nova::Utcb *>(
				Genode::Thread::myself()->utcb());
		}

		void _svm_startup()
		{
			using namespace Nova;

			Vmm::log(__func__);

			Utcb &utcb = _utcb_of_myself();
			memset(&utcb, 0x00, Utcb::size());

			utcb.mtd = 0
				| Mtd::EBSD
				| Mtd::ESP
				| Mtd::EIP
				| Mtd::EFL
				| Mtd::ESDS
				| Mtd::FSGS
				| Mtd::CSSS
				| Mtd::TR
				| Mtd::LDTR
				| Mtd::GDTR
				| Mtd::IDTR
				| Mtd::CR;

			utcb.ip    = _guest_memory._p_entry;
			utcb.flags = X86_RFLAGS_INIT;
			utcb.sp    = _guest_memory.size() - 8;
			utcb.di    = X86_BOOT_INFO_BASE;
			utcb.cr0   = X86_CR0_INIT;
			utcb.cr3   = X86_CR3_INIT;
			utcb.cr4   = X86_CR4_INIT;

			utcb.efer  = X86_EFER_INIT;
			utcb.star  = 0;
			utcb.lstar = 0;
			utcb.fmask = 0;
			utcb.kernel_gs_base = 0;

/**
 * Convert from HVT x86_sreg to NOVA format
 */
#define write_sreg(nova, hvt) \
	nova.sel = hvt.selector; \
	nova.ar  = hvt.type \
		| hvt.s   <<  4 \
		| hvt.dpl <<  5 \
		| hvt.p   <<  7 \
		| hvt.avl <<  8 \
		| hvt.l   <<  9 \
		| hvt.db  << 10 \
		| hvt.g   << 11 \
		| hvt.unusable << 12; \
	nova.limit = hvt.limit; \
	nova.base  = hvt.base; \

			write_sreg(utcb.es,   hvt_x86_sreg_data);
			write_sreg(utcb.cs,   hvt_x86_sreg_code);
			write_sreg(utcb.ss,   hvt_x86_sreg_data);
			write_sreg(utcb.ds,   hvt_x86_sreg_data);
			write_sreg(utcb.fs,   hvt_x86_sreg_data);
			write_sreg(utcb.gs,   hvt_x86_sreg_data);
			write_sreg(utcb.ldtr, hvt_x86_sreg_unusable);
			write_sreg(utcb.tr,   hvt_x86_sreg_tr);

#undef set_utcb_sreg

			utcb.gdtr.limit = X86_GDTR_LIMIT;
			utcb.gdtr.base  = X86_GDT_BASE;
			utcb.idtr.limit = 0xFFFF;

			utcb.set_msg_word(0);

			/* map the first guest memory page */
	/*
			Nova::Mem_crd crd(
				addr_t(_guest_memory.addr()) >> PAGE_SIZE_LOG2,
				GUEST_PAGE_ORDER,
				Nova::Rights(true, true, true));
			if (!utcb.append_item(crd, 0, false, true))
				Vmm::warning("initial mapping failed");
	 */
		}

		enum {
			NOVA_REQ_IRQWIN_EXIT = 0x1000U,
			IRQ_INJ_VALID_MASK   = 0x80000000UL,
			IRQ_INJ_NONE         = 0U,

			/*
			 * Intel® 64 and IA-32 Architectures Software Developer’s Manual 
			 * Volume 3C, Chapter 24.4.2.
			 * May 2012
			*/
			BLOCKING_BY_STI    = 1U << 0,
			BLOCKING_BY_MOV_SS = 1U << 1,
			ACTIVITY_STATE_ACTIVE = 0U,
			INTERRUPT_STATE_NONE  = 0U,
		};


		/*
		 * VirtualBox stores segment attributes in Intel format using a 32-bit
 		* value. NOVA represents the attributes in packed format using a 16-bit
		 * value.
		 */
		static inline Genode::uint16_t sel_ar_conv_to_nova(Genode::uint32_t v)
		{
			return (v & 0xff) | ((v & 0x1f000) >> 4);
		}


		static inline Genode::uint32_t sel_ar_conv_from_nova(Genode::uint16_t v)
		{
			return (v & 0xff) | (((uint32_t )v << 4) & 0x1f000);
		}

		//char _utcb_backup[Nova::Utcb::size()];

		Timer::Connection _debug_timer;

		Timer::Periodic_timeout<Vcpu_handler> _debug_timeout {
			_debug_timer, *this, &Vcpu_handler::_handle_debug_timeout,
			Microseconds(1000*1000*4) };

		void _handle_debug_timeout(Duration)
		{
			//Nova::Utcb &utcb = *((Nova::Utcb *)_utcb_backup);

			//log("--- _vmx_startup UTCB ---\n", utcb);

			Nova::ec_ctrl(Nova::EC_RECALL, ec_sel());
		}

		void _vmx_startup()
		{
			using namespace Nova;
			Vmm::log(__func__);

			Utcb &utcb = _utcb_of_myself();

			//memset(&utcb, 0x00, sizeof(Utcb));

			utcb.mtd = Mtd::ALL;

			utcb.ip    = 0xfff0U;
			utcb.flags = X86_RFLAGS_INIT;
			utcb.dx    = 0x600U;
			utcb.cr0   = 0x10U;

			utcb.ldtr.ar = 0x1000U;
			utcb.es.limit = 0xffffU;
			utcb.cs.sel   = 0xf000U;
			utcb.cs.limit = 0xffffU;
			utcb.cs.base  = 0xffff0000U;
			utcb.ss.limit = 0xffffU;
			utcb.ds.limit = 0xffffU;
			utcb.fs.limit = 0xffffU;
			utcb.gs.limit = 0xffffU;
			utcb.ldtr.limit = 0xffffU;
			utcb.tr.limit = 0xffffU;
			utcb.gdtr.limit     = 0xffffU;
			utcb.idtr.limit     = 0xffffU;

			//Genode::memcpy(&_utcb_backup, &utcb, sizeof(_utcb_backup));
		}

		void _vmx_invalid()
		{
			using namespace Nova;

			Vmm::error(__func__);
			_handle_debug_timeout(Duration(Microseconds(0)));

			Utcb &utcb = _utcb_of_myself();

			unsigned const dubious = utcb.inj_info |
			                         utcb.intr_state | utcb.actv_state;
			if (dubious)
				Vmm::warning(__func__, " - dubious -"
				             " inj_info=", Genode::Hex(utcb.inj_info),
				             " inj_error=", Genode::Hex(utcb.inj_error),
				             " intr_state=", Genode::Hex(utcb.intr_state),
				             " actv_state=", Genode::Hex(utcb.actv_state));
			throw Exception();
		}

		void _svm_npt()
		{
			using namespace Nova;

			Utcb &utcb = _utcb_of_myself();

			addr_t const phys_addr = utcb.qual[1] & GUEST_PAGE_MASK;
			Vmm::log(__func__, " ", (Hex)phys_addr);
			addr_t const phys_page = phys_addr >> PAGE_SIZE_LOG2;

			addr_t const local_base = _guest_memory.addr();
			addr_t const local_addr = local_base + phys_addr;
			addr_t const local_page = local_addr >> PAGE_SIZE_LOG2;

			Vmm::log(__func__, ": guest fault at ", Hex(phys_addr), "(",Hex(phys_page), "), local address is ", Hex(local_addr), "(",(Hex)local_page,")");

			Nova::Mem_crd crd(
				local_page, GUEST_PAGE_ORDER,
				Nova::Rights(true, true, true));

			addr_t const hotspot = crd.hotspot(phys_addr);

			utcb.mtd = 0;
			utcb.set_msg_word(0);

			Vmm::log(__func__, ": NPT mapping ("
				"guest=", Hex(phys_addr), ", "
				"crd.base=", Hex(crd.base()), ", "
				"hotspot=", Hex(hotspot));

			if (!utcb.append_item(crd, hotspot, false, true))
				Vmm::warning("could not map everything");
		}

		void _svm_triple()
		{
			using namespace Nova;

			Utcb &utcb = _utcb_of_myself();
			addr_t ip = utcb.ip;
			unsigned long long qual[2] = { utcb.qual[0], utcb.qual[1] };

			error("SVM triple fault exit");
			error("ip=",(Hex)ip);
			error("qual[0]=",(Hex)qual[0]);
			error("qual[1]=",(Hex)qual[1]);

			log("PDE:");
			uint64_t *pde = (uint64_t *)(_guest_memory.addr() + X86_PDE_BASE);
			size_t pages = _guest_memory.size() / X86_GUEST_PAGE_SIZE;

			for (unsigned i = 0; i < pages; ++i)
				log(i, ": ", Hex(pde[i], Hex::PREFIX, Hex::PAD));

			throw Exception();
		}

		void _svm_io()
		{
			Vmm::log(__func__);
		}

		void _gdt_access()
		{
			Vmm::log(__func__);
		}

		void _recall()
		{
			using namespace Nova;
			Utcb &utcb = _utcb_of_myself();

			addr_t ip = utcb.ip;
			addr_t addr = _guest_memory.addr()+ip;

			Vmm::log(__func__, " instruction location=", (Hex)addr, " value=", Hex(*((uint32_t*)addr)));

			utcb.mtd = 0;
		}

		/**
		 * Shortcut for calling 'Vmm::Vcpu_dispatcher::register_handler'
		 * with 'Vcpu_dispatcher' as template argument
		 */
		template <unsigned EV, void (Vcpu_handler::*FUNC)()>
		void _register_handler(Genode::addr_t exc_base, Nova::Mtd mtd)
		{
			register_handler<EV, Vcpu_handler, FUNC>(exc_base, mtd);
		}

		addr_t ec_sel()
		{
			return sel_sm_ec() + 1;
		}

		enum { STACK_SIZE = 1024*sizeof(long) };

		Vcpu_handler(Genode::Env &env,
		                Pd_connection &pd,
		                Guest_memory &memory)
		:
			Vmm::Vcpu_dispatcher<Genode::Thread>(
				env, STACK_SIZE, &env.cpu(), Affinity::Location()),
			_guest_memory(memory), _guest_pd(pd),
			_vcpu_thread(&env.cpu(), Affinity::Location(), pd.cap()),
			_debug_timer(env, "debug")
		{
			using namespace Nova;

			/* detect virtualization extension */
			Attached_rom_dataspace const info(env, "platform_info");
			Genode::Xml_node const features = info.xml().sub_node("hardware").sub_node("features");
			//PDBG(info.xml());

			bool const has_svm = features.attribute_value("svm", false);
			bool const has_vmx = features.attribute_value("vmx", false);

			/* shortcuts for common message-transfer descriptors */

			Mtd const mtd_all(Mtd::ALL);
			Mtd const mtd_cpuid(Mtd::EIP | Mtd::ACDB | Mtd::IRQ);
			Mtd const mtd_irq(Mtd::IRQ);

			Genode::addr_t exc_base = _vcpu_thread.exc_base();

			/* TODO: wrong */
			enum { QUMU_SVM = true };

			/* register virtualization event handlers */
			if (has_svm) {
				log("SVM detected");

				_register_handler<0xfe, &Vcpu_handler::_svm_startup>
					(exc_base, mtd_all);

				_register_handler<0xfc, &Vcpu_handler::_svm_npt>
					(exc_base, mtd_all);

				_register_handler<0x7f, &Vcpu_handler::_svm_triple>
					(exc_base, mtd_all);

				_register_handler<0x7b, &Vcpu_handler::_svm_io>
					(exc_base, mtd_all);

				_register_handler<0x67, &Vcpu_handler::_gdt_access>
					(exc_base, mtd_all);

				_register_handler<0x6b, &Vcpu_handler::_gdt_access>
					(exc_base, mtd_all);

				_register_handler<0xff, &Vcpu_handler::_recall>
					(exc_base, Mtd::ALL);

			} else if (has_vmx) {
				log("VMX detected");

				_register_handler<0xfe, &Vcpu_handler::_vmx_startup>
					(exc_base, Mtd::IRQ);

				_register_handler<0x21, &Vcpu_handler::_vmx_invalid>
					(exc_base, Mtd::ALL);
					
				_register_handler<0x21, &Vcpu_handler::_vmx_invalid>
					(exc_base, Mtd::ALL);

			} else {
				error("no hardware virtualization extensions available");
				throw Exception();
			}

			/* start virtual CPU */
			log("start start virtual CPU");
			_vcpu_thread.start(ec_sel());
		}
};


struct Hvt::Main
{
	Genode::Env &_env;

	Devices _devices { _env };

	Cpu_connection _guest_cpu { _env, "guest" };

	Pd_connection  _guest_pd  { _env, "guest" };

	Vmm::Vcpu_other_pd _vcpu_thread {
		&_guest_cpu, Affinity::Location(), _guest_pd.cap() };

	Guest_memory _guest_memory { _env };

	Vcpu_handler _vpu_handler { _env, _guest_pd, _guest_memory };

	Main(Genode::Env &env) : _env(env) { }
};


Genode::size_t Component::stack_size() {
	return sizeof(long)<<12; }


void Component::construct(Genode::Env &env) {
	static Hvt::Main tender(env); }


extern "C" {

void warnx(const char *s, ...)
{
	Genode::warning(s);
}

void err(int, const char *s, ...)
{
	Genode::error(s);
	Genode::sleep_forever();
}

void errx(int, const char *s, ...)
{
	Genode::error(s);
	Genode::sleep_forever();
}

void __assert(const char *, const char *file, int line, const char *failedexpr)
{
	Genode::error("Assertion failed: (", failedexpr, ") ", file, ":", line);
	Genode::sleep_forever();
};

}
