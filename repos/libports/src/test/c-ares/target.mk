TARGET  = test-libdns
LIBS    = c-ares libc posix stdcxx gtest c-ares

C_ARES_SRC_DIR := $(call select_from_ports,c-ares)/src/lib/c-ares

CC_OPT += -DHAVE_CONFIG_H -DCARES_PULL_SYS_TYPES_H
INC_DIR += $(C_ARES_SRC_DIR)

SRC_CC  = $(notdir $(wildcard $(C_ARES_SRC_DIR)/test/*.cc))

vpath %.cc $(C_ARES_SRC_DIR)/test
