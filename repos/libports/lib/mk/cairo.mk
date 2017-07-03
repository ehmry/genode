include $(REP_DIR)/lib/import/import-cairo.mk
-include $(CARIO_SRC_DIR)/Makefile.sources

LIBS += libc libm pixman zlib

CC_DEF += \
	-DCAIRO_HAS_PTHREAD \
	-DHAVE_STDINT_H \
	-D_REENTRANT \
	-DHAVE_UINT64_T \
	-UCAIRO_HAS_FT_FONT \

CC_OPT += \
	-fno-common \
	-fno-strict-aliasing \
	-Werror-implicit-function-declaration \
	-Wextra \
	-Winit-self \
	-Winline \
	-Wmissing-declarations \
	-Wmissing-format-attribute \
	-Wpacked \
	-Wp,-D_FORTIFY_SOURCE=2 \
	-Wpointer-arith \
	-Wsign-compare \
	-Wstrict-aliasing=2 \
	-Wswitch-enum \
	-Wunsafe-loop-optimizations \
	-Wvolatile-register-var \
	-Wwrite-strings \

CC_WARN += \
	-Wno-attributes \
	-Wno-long-long \
	-Wno-missing-field-initializers \
	-Wno-unused-but-set-variable \
	-Wno-unused-parameter \

SRC_C = $(cairo_sources)

vpath %.c $(CARIO_SRC_DIR)

SHARED_LIB = yes
