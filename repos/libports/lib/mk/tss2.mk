include $(REP_DIR)/lib/import/import-tss2.mk

LIBS = libc

LD_OPT  += --version-script=$(REP_DIR)/src/lib/tss2/symbol.map

SHARED_LIBRARY = yes

#SRC_CC = dummy.cc
#vpath %.cc $(REP_DIR)/src/lib/tss2
