LIBC_LOCALE_DIR = $(LIBC_DIR)/lib/libc/locale

CC_OPT += -D_Thread_local=""

FILTER_OUT = \
	c16rtomb.c c32rtomb_iconv.c mbrtoc16_iconv.c mbrtoc32_iconv.c \

SRC_C = $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(LIBC_LOCALE_DIR)/*.c)))

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(LIBC_LOCALE_DIR)

CC_CXX_WARN_STRICT =
