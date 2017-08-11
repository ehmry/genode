include $(REP_DIR)/lib/import/import-libpcg_random.mk
PCG_SRC_DIR = $(call select_from_ports,pcg-c)/src/lib/pcg-c/src

LIBS += libc

CC_OPT += -std=c99

SRC_C = $(notdir $(wildcard $(PCG_SRC_DIR)/*.c))

vpath %.c $(PCG_SRC_DIR)
