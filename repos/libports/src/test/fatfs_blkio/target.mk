TARGET = test-fatfs_blkio
LIBS = fatfs_block libc
SRC_C = app4.c
SRC_CC = component.cc

CC_DEF += -D_MAX_SS=FF_MAX_SS
CC_WARN += -Wno-pointer-to-int-cast

vpath %.c $(call select_from_ports,fatfs)/src/lib/fatfs/documents/res
