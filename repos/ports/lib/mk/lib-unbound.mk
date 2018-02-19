include $(REP_DIR)/lib/import/import-lib-unbound.mk
LIBUNBOUND_SRC_DIR := $(UNBOUND_PORT_DIR)/src/app/unbound/libunbound

INC_DIR += $(UNBOUND_PORT_DIR)/src/app/unbound $(LIBUNBOUND_SRC_DIR)

LIBS += libc libssl

SRC_C = context.c libunbound.c libworker.c

vpath %.c $(LIBUNBOUND_SRC_DIR)

SHARED_LIB = yes
