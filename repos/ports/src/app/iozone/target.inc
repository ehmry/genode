# This file is sourced for normal and noux builds of iozone

TARGET = iozone
LIBS += posix

PORT_SRC_DIR := $(call select_from_ports,iozone)/src/app/iozone/src/current

SRC_C = \
	iozone.c \
	libbif.c \

CC_DEF += \
	-DHAVE_ANSIC_C \
	-DHAVE_PREAD \
	-DMAP_ANONYMOUS=0 \
	-DNAME='"Genode"' \
	-DNO_FORK \
	-DNO_MADVISE \
	-DNO_SIGNAL \
	-DNO_THREADS \
	-Dunix \

CC_WARN += -Wno-unused-but-set-variable -Wno-misleading-indentation -Wno-implicit-int

vpath %.c $(PORT_SRC_DIR)
