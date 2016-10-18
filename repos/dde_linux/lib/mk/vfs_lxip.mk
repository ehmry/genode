SHARED_LIB = yes

LIB_DIR  = $(REP_DIR)/src/lib/vfs/lxip
INC_DIR += $(LIB_DIR)
LIBS    += lxip libc
LD_OPT  += --version-script=$(LIB_DIR)/symbol.map
SRC_CC   = vfs.cc

vpath %.cc $(REP_DIR)/src/lib/vfs/lxip
