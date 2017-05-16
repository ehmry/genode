#
# FAT File System Module using a Block session as disk I/O backend
#

FFAT_PORT_DIR := $(call select_from_ports,ffat)
FFAT_PORT_SRC_DIR := $(FFAT_PORT_DIR)/src/lib/ffat/src
FFAT_LOCAL_SRC_DIR := $(REP_DIR)/src/lib/ffat

INC_DIR += $(REP_DIR)/include/ffat $(FFAT_PORT_DIR)/include

SRC_C  = ff.c unicode.c
SRC_CC = diskio_block.cc

vpath % $(FFAT_LOCAL_SRC_DIR)
vpath % $(FFAT_PORT_SRC_DIR)
vpath % $(FFAT_PORT_SRC_DIR)/option
