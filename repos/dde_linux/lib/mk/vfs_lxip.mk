SHARED_LIB = yes

LIB_VFS_DIR  = $(REP_DIR)/src/lib/vfs/lxip
INC_DIR += $(LIB_VFS_DIR)
LIBS    += lxip
LD_OPT  += --version-script=$(LIB_VFS_DIR)/symbol.map
SRC_CC   = vfs.cc

vpath %.cc $(REP_DIR)/src/lib/vfs/lxip


LIB_DIR     = $(REP_DIR)/src/lib/lxip
LIB_INC_DIR = $(LIB_DIR)/include

LIBS += lxip_include

SETUP_SUFFIX =
CC_OPT += -DSETUP_SUFFIX=$(SETUP_SUFFIX)

CC_OPT += -U__linux__ -D__KERNEL__
CC_OPT += -DCONFIG_INET -DCONFIG_BASE_SMALL=0 -DCONFIG_DEBUG_LOCK_ALLOC \
          -DCONFIG_IP_PNP_DHCP

CC_C_OPT  += -include $(LIB_INC_DIR)/lx_emul.h
CC_CXX_OPT = -fpermissive
