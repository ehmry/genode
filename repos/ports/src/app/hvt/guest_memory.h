/*
 * \brief  HVT guest memory allocation and initialization
 * \author Emery Hemingway
 * \date   2018-09-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _HVT__GUEST_MEMORY_H_
#define _HVT__GUEST_MEMORY_H_

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <base/env.h>
#include <rm_session/connection.h>
#include <region_map/client.h>

/* VMM utilities includes */
#include <vmm/types.h>

extern "C" {

/* Upstream HVT includes */
#include "hvt.h"
#include "hvt_cpu_x86_64.h"

/* Libc includes */
#include <elf.h>

}


namespace Hvt {
	using namespace Genode;
	class Guest_memory;

	struct Invalid_guest_image : Genode::Exception { };
}


/**
 * Representation of guest memory
 *
 * The VMM and the guest share the same PD. However, the guest's view on the PD
 * is restricted to the guest-physical-to-VMM-local mappings installed by the
 * VMM for the VCPU's EC.
 *
 * The guest memory is shadowed at the lower portion of the VMM's address
 * space. If the guest (the VCPU EC) tries to access a page that has no mapping
 * in the VMM's PD, NOVA does not generate a page-fault (which would be
 * delivered to the pager of the VMM, i.e., core) but it produces a NPT
 * virtualization event handled locally by the VMM.
 */
class Hvt::Guest_memory
{
	private:

		Genode::Env                      &_env;
		Genode::Ram_dataspace_capability  _ds;
		Genode::size_t const              _local_size;
		Genode::addr_t                    _local_addr = 0;

		Genode::addr_t                    _gp_entry = 0;

		/*
		 * Noncopyable
		 */
		Guest_memory(Guest_memory const &);
		Guest_memory &operator = (Guest_memory const &);

	public:

		Guest_memory(Genode::Env &env, Genode::size_t guest_size)
		:
			_env(env),
			_ds(env.pd().alloc(guest_size)),
			_local_size(guest_size)
		{
			try {
				addr_t const local_addr =
					512*X86_GUEST_PAGE_SIZE;
				env.rm().attach_executable(_ds, local_addr);
				_local_addr = local_addr;
			}
			catch (Genode::Region_map::Region_conflict) {
				Genode::error("guest memory region conflict");
				throw;
			}

			_decode_elf();

			/* upstream utilities */
			hvt_x86_setup_gdt(local_base());
			hvt_x86_setup_pagetables(
				local_base(), local_size());

			struct hvt_boot_info &bi = boot_info();
			bi.mem_size = local_size();
			bi.cmdline = X86_CMDLINE_BASE;
		}

		~Guest_memory()
		{
			/* detache and free backing dataspace */
			_env.rm().detach((void *)_local_addr);
			_env.pd().free(_ds);
		}

		addr_t local_addr() const {
			return _local_addr; }

		uint8_t *local_base() {
			return reinterpret_cast<uint8_t *>(_local_addr); }

		size_t local_size() const {
			return _local_size; }

		size_t size() const {
			return _local_size; }

		/* guest physical entry location */
		addr_t gp_entry() const {
			return _gp_entry; }

		struct hvt_boot_info &boot_info()
		{
			return *reinterpret_cast<struct hvt_boot_info *>(
				_local_addr + X86_BOOT_INFO_BASE);
		}

		void _decode_elf()
		{
			using Genode::uint64_t;

			Attached_rom_dataspace elf_rom(_env, "guest.hvt");

			uint8_t *mem = local_base();

			size_t mem_size = local_size();

			Elf64_Phdr *phdr = nullptr;
			Elf64_Ehdr &hdr = *elf_rom.local_addr<Elf64_Ehdr>();

			/*
			 * Validate program is in ELF64 format:
			 * 1. EI_MAG fields 0, 1, 2, 3 spell ELFMAG('0x7f', 'E', 'L', 'F'),
			 * 2. File contains 64-bit objects,
			 * 3. Objects are Executable,
			 * 4. Target instruction must be set to the correct architecture.
			 */
			if (hdr.e_ident[EI_MAG0]  != ELFMAG0
			 || hdr.e_ident[EI_MAG1]  != ELFMAG1
			 || hdr.e_ident[EI_MAG2]  != ELFMAG2
			 || hdr.e_ident[EI_MAG3]  != ELFMAG3
			 || hdr.e_ident[EI_CLASS] != ELFCLASS64
			 || hdr.e_type != ET_EXEC
			 || hdr.e_machine != EM_X86_64)
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

				if ((paddr >= mem_size)
				 || add_overflow(paddr, filesz, result)
				 || (result >= mem_size))
				 	throw Invalid_guest_image();

				if (add_overflow(paddr, memsz, result)
				 || (result >= mem_size))
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
				if (_end > boot_info().kernel_end)
					boot_info().kernel_end = _end;

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

			_gp_entry = hdr.e_entry;
		}
};

#endif /* _HVT__GUEST_MEMORY_H_ */
