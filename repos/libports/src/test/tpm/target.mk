include $(REP_DIR)/lib/import/import-tss2.mk
TARGET = test-tpm
LIBS = tss2 libc

INC_DIR += \
	$(TSS_PORT_DIR)/test/common/sample \
	$(TSS_PORT_DIR)/test/tpmclient \
	$(TSS_PORT_DIR)/common \
	$(TSS_PORT_DIR)/include/sapi \
	$(TSS_PORT_DIR)/sysapi/include \
	$(TSS_PORT_DIR)/resourcemgr \

SRC_CC := $(TSS_PORT_DIR)/test/tpmtest/tpmtest.cpp
