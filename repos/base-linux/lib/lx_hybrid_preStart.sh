#
# Manually supply all library search paths of the host compiler to our tool
# chain.
#
HOST_LIB_SEARCH_DIRS=$(cc $ccMarch -print-search-dirs | grep libraries | \
                              sed -e "s/.*=//" -e "s/:/ /g" -e "s/\/ / /g" -e "s/\/\$$//")


#
# Add search path for 'limits.h'
#
# We cannot simply extend 'INC_DIR' because this would give precedence to the
# host include search paths over Genode search path. The variable HOST_INC_DIR
# is appended to the include directory list.
#
#HOST_INC_DIR=$(echo "int main() {return 0;}" |\
#                      LANG=C $cxx -x c++ -v -E - 2>&1 |\
#                        sed '/^\#include <\.\.\.> search starts here:/,/^End of search list/!d' |\
#                        grep "include-fixed")

#
# Add search paths for normal libraries
#
for d in $HOST_LIB_SEARCH_DIRS
do cxxLinkFlags="$cxxLinkFlags -L$d"
done


#
# Add search paths for shared-library lookup
#
# We add all locations of shared libraries present in the ld.cache to our
# library search path.
#
#HOST_SO_SEARCH_DIRS := $(sort $(dir $(shell $(LDCONFIG) -p | sed "s/^.* \//\//" | grep "^\/")))
#LINK_ARG_PREFIX := -Wl,
#CXX_LINK_OPT += $(addprefix $(LINK_ARG_PREFIX)-rpath-link $(LINK_ARG_PREFIX),$(HOST_SO_SEARCH_DIRS))

#
# The '__libc_csu_init' function is normally provided by the C library. We
# override the libc's version in our 'lx_hybrid' library to have a hook for
# Genode-specific initializations. Unfortunately, this way, we get two symbols
# with the same name. So we have to tell the linker to be forgiving. The order
# of the libraries at the linker command line determines which symbol is used.
# Therefore it is important to have 'lx_hybrid.lib.so' listed before '-lc',
# which is always the case when supplying '-lc' via 'EXT_OBJECTS' (not
# 'CXX_LINK_OPT').
#
extraLdFlags="$extraLdFlags --allow-multiple-definition"

#
# Make exceptions work
#
extraFlags="$extraFlags -Wl,--eh-frame-hdr"


#
# Add all libraries and their dependencies specified at the 'lxLibs'
# variable to the linker command line
#
for l in $lxLibs; do
    objects="$objects $(pkg-config --libs $l/lib/pkgconfig/*.pc)"
done


#
# Use the host's startup codes, linker script, and dynamic linker
#
#ifneq ($(filter hardening_tool_chain, $(SPECS)),)
#objects=$objects $( cc $ccMarch -print-file-name=Scrt1.o)
#objects=$objects $( cc $ccMarch -print-file-name=crti.o)
#objects=$objects $( $cc $ccMarch -print-file-name=crtbeginS.o)
#objects=$objects $( $cc $ccMarch -print-file-name=crtendS.o)
#else
objects="$objects $(cc $ccMarch -print-file-name=crt1.o)"
objects="$objects $(cc $ccMarch -print-file-name=crti.o)"
objects="$objects $($cc $ccMarch -print-file-name=crtbegin.o)"
objects="$objects $($cc $ccMarch -print-file-name=crtend.o)"
#endif
objects="$objects $(cc $ccMarch -print-file-name=crtn.o)"
objects="$objects -lgcc -lgcc_s -lsupc++ -lc -lpthread"

#USE_HOST_LD_SCRIPT = yes

#ifeq (x86_64,$(findstring x86_64,$(SPECS)))
#CXX_LINK_OPT += -Wl,--dynamic-linker=/lib64/ld-linux-x86-64.so.2
#else
#ifeq (arm,$(findstring arm,$(SPECS)))
#CXX_LINK_OPT += -Wl,--dynamic-linker=/lib/ld-linux.so.3
#else
#CXX_LINK_OPT += -Wl,--dynamic-linker=/lib/ld-linux.so.2
#endif
#endif

dynamicLinker="$(cat $gcc/nix-support/dynamic-linker)"

# because we use the host compiler's libgcc, omit the Genode toolchain's version
#LD_LIBGCC =

# use the host c++ for linking to find shared libraries in DT_RPATH library paths
cxx="c++"

unset ldScripts

# Remove cxx from libs (libs are found by globbing
# so an invalid path is not a problem).
libs="${libs/cxx/}"
