LIBC_RESOLV_DIR = $(LIBC_DIR)/lib/libc/resolv

SRC_C = $(notdir $(wildcard $(LIBC_RESOLV_DIR)/*.c))

include $(REP_DIR)/lib/mk/libc-common.inc
INC_DIR += $(LIBC_DIR)/sys/sys

vpath %.c $(LIBC_RESOLV_DIR)

CC_CXX_WARN_STRICT =
