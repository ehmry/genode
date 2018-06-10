DEPOT_DIR ?= $(GENODE_DIR)/depot
DEPOT_USER ?= genodelabs

DEPOT_CC  = /usr/local/genode-gcc/bin/genode-x86-gcc
DEPOT_CXX = /usr/local/genode-gcc/bin/genode-x86-g++
DEPOT_LD  = /usr/local/genode-gcc/bin/genode-x86-ld

CC  ?= $(DEPOT_CC)
CXX ?= $(DEPOT_CXX)
LD  ?= $(DEPOT_LD)

DEPOT_LD_SCRIPT_SO ?= $(GENODE_DIR)/repos/base/src/ld/genode_rel.ld

DEPOT_LDFLAGS := -shared --eh-frame-hdr \
	-melf_x86_64 -gc-sections -z max-page-size=0x1000 \
	-T $(DEPOT_LD_SCRIPT_SO) --entry=0x0

# find a suitable api package
depot_api_dir = $(lastword $(wildcard $(DEPOT_DIR)/$(DEPOT_USER)/api/$(1)/*))

#
# Include directories
#

_api_dir_includes = $(wildcard $(1)/include $(1)/include/*)

depot_includes = $(foreach API,$1,\
	$(foreach DIR,$(call _api_dir_includes,\$(call depot_api_dir,$(API))),\
		-I$(DIR)))

#
# Stub libraries
#

depot_symbol_files = $(foreach API,$1,\
	$(wildcard $(call _api_dir,$(API))/lib/symbols/*))

depot_abi_libraries = $(foreach API,$1,$(API).lib.so)

%.lib.so: %.symbols.o
	$(DEPOT_LD) $(ABI_SO) -shared --eh-frame-hdr $(LD_OPT) \
		-T $(DEPOT_LD_SCRIPT_SO) \
		--whole-archive --start-group \
		$< \
		--end-group --no-whole-archive \
		-o $@

%.symbols.o: %.symbols.s
	$(CC) -c $< -o $@

%.symbols.s: %.symbols
	sed \
		-e "s/^\(\w\+\) D \(\w\+\)\$$/.data; .global \1; .type \1,%object; .size \1,\2; \1:/p" \
		-e "s/^\(\w\+\) V/.data; .weak \1; .type \1,%object; \1:/p" \
		-e "s/^\(\w\+\) T/.text; .global \1; .type \1,%function; \1:/p" \
		-e "s/^\(\w\+\) R \(\w\+\)\$$/.section .rodata; .global \1; .type \1,%object; .size \1,\2; \1:/p" \
		-e "s/^\(\w\+\) W/.text; .weak \1; .type \1,%function; \1:/p" \
		-e "s/^\(\w\+\) B \(\w\+\)\$$/.bss; .global \1; .type \1,%object; .size \1,\2; \1:/p" \
		-e "s/^\(\w\+\) U/.text; .global \1; $(ASM_SYM_DEPENDENCY)/p" \
		$< > $@

%.symbols: /home/repo/genode/depot/genodelabs/api/libc/2018-05-28/lib/symbols/%
	ln -sv $< $@
