SRC_CC = vfs_ffat.cc

INC_DIR += $(REP_DIR)/src/lib/vfs/ffat

LIBS  += ffat_block

vpath %.cc $(REP_DIR)/src/lib/vfs/ffat

SHARED_LIB = yes
