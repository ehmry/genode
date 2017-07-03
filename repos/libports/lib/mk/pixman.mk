include $(REP_DIR)/lib/import/import-pixman.mk
-include $(PIXMAN_SRC_DIR)/Makefile.sources

LIBS += libc

CC_DEF += -DPACKAGE=\"pixman\" -DPIXMAN_NO_TLS

INC_DIR += $(PIXMAN_SRC_DIR)

SRC_C = $(libpixman_sources)

vpath %.c $(PIXMAN_SRC_DIR)
