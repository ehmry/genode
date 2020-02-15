ifeq (@(TUP_ARCH),arm)
SRC_S += $(BASE_DIR)/src/lib/base-common/arm/*.S
endif

ifeq (@(TUP_ARCH),arm64)
SRC_S += $(BASE_DIR)/src/lib/base-common/arm_64/*.S
endif

ifeq (@(TUP_ARCH),i386)
SRC_S += $(BASE_DIR)/src/lib/base-common/x86_32/*.S
endif

ifeq (@(TUP_ARCH),x86_64)
SRC_S += $(BASE_DIR)/src/lib/base-common/x86_64/*.S
endif
