SRC_CC = vfs.cc

INC_DIR += $(REP_DIR)/src/lib/vfs/lwip

LIBS  += lwip

vpath %.cc $(REP_DIR)/src/lib/vfs/lwip

SHARED_LIB = yes
