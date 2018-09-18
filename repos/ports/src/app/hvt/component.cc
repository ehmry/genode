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

	struct Devices;
	struct Exit_dispatcher;
	struct Guest_memory;
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

	template <typename T>
	class Vcpu_dispatcher;
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
				"solo5");
			_nic->rx_channel()->sigh_packet_avail(_nic_handler);

			Net::Mac_address nic_mac = _nic->mac_address();

			Genode::memcpy(
				_nic_info.mac_address, nic_mac.addr,
				min(sizeof(_nic_info.mac_address), sizeof(nic_mac.addr)));
		}

		if (config.has_sub_node("blk")) {
			_block.construct(env, &_pkt_alloc, 128*1024, "solo5");

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


struct Hvt::Exit_dispatcher {

		static Nova::Utcb *_utcb_of_myself() {
			return reinterpret_cast<Nova::Utcb *>(Genode::Thread::myself()->utcb()); }

		void _vcpu_startup()
		{
			using namespace Nova;

			Vmm::log(__func__, " called");

			Utcb *utcb = _utcb_of_myself();
			memset(utcb, 0x00, sizeof(Utcb));

			utcb->mtd =
				Mtd::EBSD | Mtd::ESP | Mtd::EIP | Mtd::EFL |
				Mtd::ESDS | Mtd::FSGS | Mtd::CSSS |
				Mtd::TR | Mtd::LDTR | Mtd::GDTR  | Mtd::IDTR |
				Mtd::EFER;

			// TODO: try Mtd::ALL

			// TODO: utcb->ip = _guest_memory._p_entry;
			utcb->flags = X86_RFLAGS_INIT;
			// TODO: utcb->sp = _guest_memory._guest_ram_size - 8;
			utcb->di = X86_BOOT_INFO_BASE;
			utcb->cr0 = X86_CR0_INIT;
			utcb->cr3 = X86_CR3_INIT;
			utcb->cr4 = X86_CR4_INIT;

			utcb->efer = X86_EFER_INIT;
			utcb->star = 0;
			utcb->lstar = 0;
			utcb->fmask = 0;
			utcb->kernel_gs_base = 0;

#define sreg_to_utcb(in, out) \
	out.sel = in.selector * 8; \
	out.ar = in.type \
		| in.s <<  4 | in.dpl <<  5 | in.p <<  7 \
		| in.l << 13 | in.db  << 14 | in.g << 15 \
		| in.unusable << X86_SREG_UNUSABLE_BIT; \
	out.limit = in.limit; \
	out.base = in.base; \

			sreg_to_utcb(hvt_x86_sreg_data, utcb->es);
			sreg_to_utcb(hvt_x86_sreg_code, utcb->cs);
			sreg_to_utcb(hvt_x86_sreg_data, utcb->ss);
			sreg_to_utcb(hvt_x86_sreg_data, utcb->ds);
			sreg_to_utcb(hvt_x86_sreg_data, utcb->fs);
			sreg_to_utcb(hvt_x86_sreg_data, utcb->gs);
			sreg_to_utcb(hvt_x86_sreg_unusable, utcb->ldtr);
			sreg_to_utcb(hvt_x86_sreg_tr, utcb->tr);

#undef sreg_to_utcb

			utcb->gdtr.limit = X86_GDTR_LIMIT;
			utcb->gdtr.base = X86_GDT_BASE;

			utcb->idtr.limit = 0xffff;
			utcb->idtr.base = 0;
		}

		void _svm_npt()
		{
			using namespace Nova;

			Utcb *utcb = _utcb_of_myself();
			addr_t const q0 = utcb->qual[0];
			addr_t const vm_fault_addr = utcb->qual[1];

			auto ip = (addr_t)utcb->ip;
			auto sp = (addr_t)utcb->sp;

			log(__func__, " called, request mapping at ", (Hex)vm_fault_addr, " (", (Hex)q0, ")");
			log("IP is ", (Hex)ip);
			log("SP is ", (Hex)sp);

			sleep_forever();
		}


	Exit_dispatcher()
	{
			using namespace Nova;

			/* shortcuts for common message-transfer descriptors */
/*
			Mtd const mtd_all(Mtd::ALL);
			Mtd const mtd_cpuid(Mtd::EIP | Mtd::ACDB | Mtd::IRQ);
			Mtd const mtd_irq(Mtd::IRQ);

			Genode::addr_t exc_base = _vcpu_thread.exc_base();
*/
			/* register virtualization event handlers */
/*
			if (type == SVM) {
				_register_handler<0xfe, &This::_vcpu_startup>
					(exc_base, mtd_all);

				_register_handler<0xfc, &This::_svm_npt>
					(exc_base, mtd_all);

			}
*/
			/* start virtual CPU */
			//_vcpu_thread.start(sel_sm_ec() + 1);
	
	}
};


struct Hvt::Guest_memory
{
	Genode::Env &_env;

	Attached_ram_dataspace _guest_ram;
	size_t const           _guest_ram_size = _guest_ram.size();

	/* elf entry point (on physical memory) */
	hvt_gpa_t _p_entry = 0;

	/* highest byte of the program (on physical memory) */
	hvt_gpa_t _p_end   = 0;

	void _load_elf(Genode::Env &env)
	{
		Attached_rom_dataspace elf_rom(env, "guest");

		uint8_t *mem    = _guest_ram.local_addr<uint8_t>();
		size_t mem_size = _guest_ram.size();

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
		    size_t offset = phdr[ph_i].p_offset;
		    size_t filesz = phdr[ph_i].p_filesz;
		    size_t memsz = phdr[ph_i].p_memsz;
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
		    memset(daddr + filesz, 0, memsz - filesz);

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

		/*
		    if (mprotect(daddr, _end - paddr, prot) == -1)
		        throw Invalid_guest_image();
		 */
		}

		_p_entry = hdr.e_entry;
		return;
	}

	Guest_memory(Genode::Env &env, size_t size)
	: _env(env), _guest_ram(env.pd(), env.rm(), size)
	{
		log("Load guest ELF...");
		_load_elf(env);
		log("Guest ELF valid! entry=", (Hex)_p_entry, ", end=", (Hex)_p_end);
	}

	size_t size() const {
		return _guest_ram_size; }
};


struct Hvt::Main
{
	Genode::Env &_env;

	Devices _devices { _env };

	Cpu_connection _guest_cpu { _env, "guest" };

	Pd_connection  _guest_pd  { _env, "guest" };

	size_t guest_size()
	{
		Genode::log("reserve stack virtual space");
		Vmm::Virtual_reservation reservation(_env, Genode::Thread::stack_area_virtual_base());

		size_t size = _env.ram().avail_ram().value;
		size -= 1 << 20;
		size = size & ~((1UL << Vmm::PAGE_SIZE_LOG2) - 1);

		Genode::log("guest size will be ", (Hex)size, " (", size>>20, " MiB)");
		return size;
	}

	Guest_memory _guest_memory { _env, guest_size() };

	Vmm::Virtual_reservation _reservation { _env, _guest_memory.size() };

	typedef Vcpu_dispatcher<Vmm::Vcpu_other_pd> Vcpu;

	//Vcpu _vcpu { _env, Vcpu::SVM, "hvt-guest", _env_pd.cap(), _guest_memory };

	Vmm::Vcpu_other_pd _vcpu_thread {
		&_guest_cpu, Affinity::Location(), _guest_pd.cap() };

	Main(Genode::Env &env) : _env(env) { }
};

void Component::construct(Genode::Env &env)
{
	using namespace Genode;


	static Hvt::Main tender(env);
}
