LIBS += libc libssl

UNBOUND_PORT_DIR := $(call select_from_ports,unbound)
UNBOUND_SRC_DIR := $(UNBOUND_PORT_DIR)/src/app/unbound
UNBOUND_SLDNS_SRC_DIR := $(UNBOUND_SRC_DIR)/sldns

INC_DIR += $(UNBOUND_SRC_DIR)

SRC_C += $(notdir $(wildcard $(UNBOUND_SLDNS_SRC_DIR)/*.c))

vpath %.c $(UNBOUND_SLDNS_SRC_DIR)
