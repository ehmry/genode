REQUIRES   = linux
SRC_CC     = vfs.cc
LIBS      += lx_hybrid

INC_DIR += $(REP_DIR)/src/lib/vfs/linux
vpath %.cc $(REP_DIR)/src/lib/vfs/linux

SHARED_LIB = yes
