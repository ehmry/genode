#
# FAT File System Module using a Block session as disk I/O backend
#

FFAT_PORT_DIR := $(call select_from_ports,fatfs)
FFAT_PORT_SRC_DIR := $(FFAT_PORT_DIR)/src/lib/fatfs/source
FFAT_LOCAL_SRC_DIR := $(REP_DIR)/src/lib/fatfs

INC_DIR += $(REP_DIR)/include/fatfs $(FFAT_PORT_DIR)/include

SRC_C  = ff.c ffunicode.c
SRC_CC = diskio_block.cc

vpath % $(FFAT_LOCAL_SRC_DIR)
vpath % $(FFAT_PORT_SRC_DIR)
