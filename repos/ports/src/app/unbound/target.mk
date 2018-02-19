TARGET = unbound
LIBS += posix libssl lib-unbound unbound-common unbound-sldns

UNBOUND_PORT_DIR := $(call select_from_ports,unbound)
UNBOUND_SRC_DIR := $(UNBOUND_PORT_DIR)/src/app/unbound
UNBOUND_DAEMON_SRC_DIR := $(UNBOUND_SRC_DIR)/daemon

INC_DIR += $(UNBOUND_DAEMON_SRC_DIR)/.. $(UNBOUND_DAEMON_SRC_DIR)

CC_DEF += -DRETSIGTYPE=void

SRC_C += $(notdir $(wildcard $(UNBOUND_DAEMON_SRC_DIR)/*.c))

vpath %.c $(UNBOUND_DAEMON_SRC_DIR)
