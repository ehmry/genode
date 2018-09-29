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

/* local includes */
//#include "devices.h"

namespace Hvt {
	using namespace Genode;
	using Genode::uint64_t;

	/* used to create 2 MiB mappings */
	enum {
		GUEST_PAGE_MASK = ~(X86_GUEST_PAGE_SIZE-1),
		GUEST_PAGE_ORDER = 9
	};

	struct Guest_memory;
	struct Vcpu_handler;
	struct Main;

	struct Invalid_guest_image : Genode::Exception { };

};


struct Hvt::Guest_memory
{
	Genode::Env &_env;

	size_t _guest_quota()
	{
		size_t avail_ram = _env.pd().avail_ram().value;
		size_t avail_pages = min(size_t(512), avail_ram / size_t(X86_GUEST_PAGE_SIZE));
		if (!avail_pages) {
			error("insufficient RAM quota to host an HVT guest");
			throw Out_of_ram();
		}
		return avail_pages * X86_GUEST_PAGE_SIZE;
	}

	size_t const _guest_ram_size = _guest_quota();

	Ram_dataspace_capability _ram_ds_cap =
		_env.pd().alloc(_guest_ram_size-0x1000);

/*
	Attached_ram_dataspace _ram_ds {
		_env.pd(), _env.rm(), _guest_ram_size };
 */

	/* elf entry point (on physical memory) */
	hvt_gpa_t _p_entry = 0;

	/* highest byte of the program (on physical memory) */
	hvt_gpa_t _p_end   = 0;

	void _load_elf(Genode::Env &env)
	{
		Attached_rom_dataspace elf_rom(env, "guest");

		uint8_t *mem = 0; //_ram_ds.local_addr<uint8_t>();
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
		/* attach guest RAM at second page in local address space */
		_env.rm().attach_at(_ram_ds_cap, 0x1000);

		log("Load guest ELF...");
		_load_elf(env);
		log("Guest ELF valid! entry=", (Hex)_p_entry, ", end=", (Hex)_p_end);

		/* initialize GDT */
			hvt_x86_setup_gdt(base());

		/* initialize the page tables */
		hvt_x86_setup_pagetables(base(), size());

		enum { TODO_TSC_GUESS = 7 };

		struct hvt_boot_info *bi =
			(struct hvt_boot_info *)(base() + X86_BOOT_INFO_BASE);
		bi->mem_size = _guest_ram_size;
		bi->kernel_end = _p_end;
		bi->cmdline = X86_CMDLINE_BASE;
		bi->cpu.tsc_freq = TODO_TSC_GUESS;

		char *cmdline = (char *)X86_CMDLINE_BASE; //_ram_ds.local_addr<char>()+X86_CMDLINE_BASE;
		Genode::snprintf(cmdline, HVT_CMDLINE_SIZE, "NOVA HVT");
	}

	uint8_t *base() const {
		return 0; } //_ram_ds.local_addr<uint8_t>(); }

	addr_t addr() const {
		return 0; } //(addr_t)_ram_ds.local_addr<uint8_t>(); }

	size_t size() const {
		return _guest_ram_size; }

	void dump_page_tables() const
	{
		Vmm::log("PDE:");
		uint64_t *pde = (uint64_t *)(addr() + X86_PDE_BASE);
		size_t pages = size() / X86_GUEST_PAGE_SIZE;

		for (unsigned i = 0; i < pages; ++i)
			Vmm::log(i, ": ", Hex(pde[i], Hex::PREFIX, Hex::PAD));
	}
};


struct Hvt::Vcpu_handler : Vmm::Vcpu_dispatcher<Genode::Thread>
{
		Guest_memory        &_guest_memory;
		Vmm::Vcpu_same_pd   _vcpu_thread;

		static Nova::Utcb &_utcb_of_myself()
		{
			return *reinterpret_cast<Nova::Utcb *>(
				Genode::Thread::myself()->utcb());
		}

		void _vcpu_init(Nova::Utcb &utcb)
		{
			using namespace Nova;

			memset(&utcb, 0x00, Utcb::size());
			utcb.mtd = 0xfffff;

			utcb.ip = 0xfff0;
			utcb.flags = 0x2; // reserved

			utcb.dx = 0x600; // Seoul
			utcb.cr0 = 0x10;

			utcb.dr7 = 0x400;

			utcb.cs.sel  = 0xf000;
			utcb.cs.base = 0xffff0000;

			utcb.es.ar =
			utcb.cs.ar =
			utcb.ss.ar =
			utcb.ds.ar =
			utcb.fs.ar =
			utcb.gs.ar = 0x93;

			utcb.es.limit =
			utcb.cs.limit =
			utcb.ss.limit =
			utcb.ds.limit =
			utcb.fs.limit =
			utcb.gs.limit =
			utcb.ldtr.limit =
			utcb.tr.limit =
			utcb.idtr.limit = 0xffff;

			utcb.ldtr.ar = 0x1000;
			utcb.tr.ar = 0x8b;

			utcb.gdtr.limit = X86_GDTR_LIMIT;
			utcb.gdtr.base  = X86_GDT_BASE;
		}

		void _vcpu_hvt_init(Nova::Utcb &utcb)
		{
			using namespace Nova;

			_vcpu_init(utcb);

			utcb.mtd |= 0
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

			// Basic CPU control in CR0
			utcb.cr0 = X86_CR0_PE | X86_CR0_NE;
			//utcb.cr0 = X86_CR0_INIT;

			// PML4
			utcb.cr3   = X86_CR3_INIT;

			// Intel CPU features in CR4
			utcb.cr4   = X86_CR4_INIT;

			// Long-mode
			utcb.efer  = X86_EFER_LME | X86_EFER_LMA;
			// set LME but not LMA?

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

			write_sreg(utcb.cs,   hvt_x86_sreg_code);
			write_sreg(utcb.es,   hvt_x86_sreg_data);
			write_sreg(utcb.ss,   hvt_x86_sreg_data);
			write_sreg(utcb.ds,   hvt_x86_sreg_data);
			write_sreg(utcb.fs,   hvt_x86_sreg_data);
			write_sreg(utcb.gs,   hvt_x86_sreg_data);
			write_sreg(utcb.ldtr, hvt_x86_sreg_unusable);
			write_sreg(utcb.tr,   hvt_x86_sreg_tr);

			#undef set_utcb_sreg

			utcb.gdtr.limit = X86_GDTR_LIMIT;
			utcb.gdtr.base  = X86_GDT_BASE;

			/* this is done on OpenBSD */
			utcb.idtr.limit = 0xffff;

			/* initialize GDT in guest */
			hvt_x86_setup_gdt(_guest_memory.base());

			/* initialize the page tables in guest memory */
			hvt_x86_setup_pagetables(
				_guest_memory.base(), _guest_memory.size());

			/* map the guest pages */
			utcb.set_msg_word(0);
			for (addr_t phys = 0;
			     phys < _guest_memory.size();
			     phys += X86_GUEST_PAGE_SIZE)
			{
				Nova::Mem_crd crd(
					(_guest_memory.addr() + phys) >> PAGE_SIZE_LOG2,
					GUEST_PAGE_ORDER, Nova::Rights(true, true, true));
				if (!utcb.append_item(crd, phys, false, true)) {
					Vmm::warning("initial mapping failed");
					break;
				}
			}
		}

		void _svm_startup()
		{
			using namespace Nova;

			Utcb &utcb = _utcb_of_myself();
			_vcpu_hvt_init(utcb);

			/* set false segment register bases to identify the first page fault */

			Genode::memcpy(&_utcb_backup, &utcb, sizeof(_utcb_backup));
		}

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

		char _utcb_backup[Nova::Utcb::size()];

		Timer::Connection _debug_timer;

/*
		Timer::Periodic_timeout<Vcpu_handler> _debug_timeout {
			_debug_timer, *this, &Vcpu_handler::_handle_debug_timeout,
			Microseconds(1000*1000*4) };
 */

		void _dump_utcb_backup()
		{
			Nova::Utcb &utcb = *((Nova::Utcb *)_utcb_backup);
			Vmm::log("--- _vmx_startup UTCB ---\n", utcb);
		}

		void _handle_debug_timeout(Duration)
		{
			//_dump_utcb_backup;

			Nova::ec_ctrl(Nova::EC_RECALL, ec_sel());
		}

		void _vmx_triple()
		{
			using namespace Nova;

			Vmm::error(__func__);

			//_dump_utcb_backup();

    		struct x86_gdt_desc *gdt = (struct x86_gdt_desc *)
				(_guest_memory.base() + X86_GDT_BASE);
    		uint64_t *raw = (uint64_t *)
				(_guest_memory.base() + X86_GDT_BASE);

			for (unsigned i = 0; i < X86_GDT_MAX; ++i) {
				log("GDT ", i, ": ", (Hex)raw[i]);
				log("       type=", (Hex)gdt[i].type,
					" base=", (Hex)((gdt[i].base_hi<<24) | (gdt[i].base_lo)));
			}

			_guest_memory.dump_page_tables();

			throw Exception();
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

			{
				Nova::Utcb &u = *((Nova::Utcb *)_utcb_backup);
				log(u);
			}

			//_dump_utcb_backup();
			throw Exception();
		}

		void _vmx_startup()
		{
			using namespace Nova;
			Vmm::log(__func__);

			Utcb &utcb = _utcb_of_myself();

			_vcpu_hvt_init(utcb);

			utcb.cr4 |= X86_CR4_VMXE;

			Genode::memcpy(&_utcb_backup, &utcb, sizeof(_utcb_backup));
		}

		__attribute__((noreturn))
		void _svm_npt()
		{
			using namespace Nova;

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
			[init -> hvt] _svm_npt 0x0 0x2000
			[init -> hvt] _svm_npt 0x0 0x3400
			[init -> hvt] _svm_npt 0x0 0x4070
			 */

			Utcb &utcb = _utcb_of_myself();

			if (utcb.actv_state != ACTIVITY_STATE_ACTIVE) {
				Vmm::log(__func__, " qual[1]=", (Hex)utcb.qual[1], " actv_state=", (Hex)utcb.actv_state);
			}

			if (utcb.inj_info & IRQ_INJ_VALID_MASK) {
				Vmm::error("EPT violation during IDT vectoring, ", (Hex)utcb.qual[1]);
				enum {
					INJ_IRQWIN = 0x1000,
					INJ_NMIWIN = 0x0000, // XXX missing
					INJ_WIN    = INJ_IRQWIN | INJ_NMIWIN
				};
			}

			addr_t const gpa = utcb.qual[1];
			Vmm::log(__func__, " ", (Hex)gpa);

			utcb.mtd = 0;
			utcb.set_msg_word(0);

			/* map a single page */
			Nova::Mem_crd crd(
				gpa>>PAGE_SIZE_LOG2, 0, Nova::Rights(true, true, true));
			if (!utcb.append_item(crd, gpa, false, true)) {
				Vmm::warning(__func__, " failed");
			}
			Vmm::log(__func__, " reply");
		}

		void _vmx_ept()
		{
			using namespace Nova;

			Utcb &utcb = _utcb_of_myself();

			addr_t const phys_addr = utcb.qual[1];

			Vmm::log(__func__, " ", (Hex)phys_addr);
		}

		void _svm_exception()
		{
			Vmm::error(__func__, " not handled");
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

			_guest_memory.dump_page_tables();

			sleep_forever();
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

		Vcpu_handler(Genode::Env &env, Guest_memory &memory)
		:
			Vmm::Vcpu_dispatcher<Genode::Thread>(
				env, STACK_SIZE, &env.cpu(), Affinity::Location()),
			_guest_memory(memory),
			_vcpu_thread(&env.cpu(),
			             Affinity::Location(),
			             env.pd_session_cap(),
			             1024*sizeof(Genode::addr_t)),
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

				_register_handler<0x4e, &Vcpu_handler::_svm_exception>
					(exc_base, mtd_all);

				_register_handler<0x7f, &Vcpu_handler::_svm_triple>
					(exc_base, Mtd::ALL);

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

				_register_handler<0x02, &Vcpu_handler::_vmx_triple>
					(exc_base, Mtd::ALL);

				_register_handler<0x21, &Vcpu_handler::_vmx_invalid>
					(exc_base, Mtd::ALL);

				_register_handler<0x30, &Vcpu_handler::_vmx_ept>
					(exc_base, Mtd::ALL);

				_register_handler<0xfe, &Vcpu_handler::_vmx_startup>
					(exc_base, 0);

			} else {
				error("no hardware virtualization extensions available");
				throw Exception();
			}

			/* start virtual CPU */
			log("start virtual CPU");
			_vcpu_thread.start(ec_sel());
		}
};


struct Hvt::Main
{
	Genode::Env &_env;

	//Devices _devices { _env };

	Guest_memory _guest_memory { _env };

	Vcpu_handler _vpu_handler { _env, _guest_memory };

	Main(Genode::Env &env) : _env(env) { }
};


Genode::size_t Component::stack_size() {
	return sizeof(long)<<12; }


void Component::construct(Genode::Env &env)
{
	static Hvt::Main tender(env);
}


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
