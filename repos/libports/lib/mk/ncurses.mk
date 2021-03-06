NCURSES_PORT_DIR := $(call select_from_ports,ncurses)
NCURSES_SRC_DIR  := $(NCURSES_PORT_DIR)/src/lib/ncurses/ncurses

# files from the 'ncurses/base/' subdirectory
ALL_BASE_SRC_C  = $(notdir $(wildcard $(NCURSES_SRC_DIR)/base/*.c))
SRC_C += $(filter-out sigaction.c lib_driver.c,$(ALL_BASE_SRC_C))
vpath %.c $(NCURSES_SRC_DIR)/base

# files from the 'ncurses/tinfo/' subdirectory
ALL_TINFO_SRC_C = $(notdir $(wildcard $(NCURSES_SRC_DIR)/tinfo/*.c))
SRC_C += $(filter-out make_hash.c make_keys.c tinfo_driver.c,$(ALL_TINFO_SRC_C))
vpath %.c $(NCURSES_SRC_DIR)/tinfo

# files from the 'ncurses/tty/' subdirectory
ALL_TTY_SRC_C = $(notdir $(wildcard $(NCURSES_SRC_DIR)/tty/*.c))
SRC_C += $(ALL_TTY_SRC_C)
vpath %.c $(NCURSES_SRC_DIR)/tty
# prevent the use of '\n' for 'cursor down' movement, because the terminal adds
# CR by default and ncurses' clearing of 'ONLCR' from 'c_oflag' currently has
# no effect
CC_OPT_lib_mvcur = -DNO_OPTIMIZE

# files from the 'ncurses/trace/' subdirectory
SRC_C += $(notdir $(addprefix $(NCURSES_SRC_DIR)/trace/,lib_trace.c varargs.c visbuf.c))
vpath %.c $(NCURSES_SRC_DIR)/trace

# files generated by 'prepare_port'
SRC_C += $(notdir $(wildcard $(NCURSES_PORT_DIR)/src/lib/ncurses/*.c))
vpath %.c $(NCURSES_PORT_DIR)/src/lib/ncurses

INC_DIR += $(NCURSES_SRC_DIR)
INC_DIR += $(NCURSES_PORT_DIR)/include/ncurses
INC_DIR += $(REP_DIR)/include/ncurses

LIBS += libc

SHARED_LIB = yes


CC_CXX_WARN_STRICT =
