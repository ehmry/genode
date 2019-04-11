LIBC_GEN_DIR = $(LIBC_DIR)/lib/libc/gen

# this file produces a warning about a missing header file, lets drop it
FILTER_OUT_C += getosreldate.c sem.c valloc.c getpwent.c

# the following are superceded by our minimalist sysctl and sysconf
FILTER_OUT_C += \
	devname.c feature_present.c getpagesizes.c getvfsbyname.c \
	setproctitle.c sysconf.c sysctlbyname.c

# the following is a wrapper over rusage, which we cannot provide
FILTER_OUT_C += clock.c

# avoid chacha20 dependency for simplicity's sake
FILTER_OUT_C += arc4random.c

# we implement this ourselves
FILTER_OUT_C += isatty.c

FILTER_OUT_C += \
	disklabel.c \
	exect.c \
	cap_sandboxed.c \
	getutxent.c \
	pututxline.c \
	readpassphrase.c \
	sem_new.c \
	utxdb.c \
	trivial-getcontextx.c \
	scandir_b.c \

SRC_C = $(filter-out $(FILTER_OUT_C),$(notdir $(wildcard $(LIBC_GEN_DIR)/*.c)))

# 'sysconf.c' includes the local 'stdtime/tzfile.h'
INC_DIR += $(REP_DIR)/src/lib/libc/stdtime

# '_pthread_stubs.c' includes the local 'libc_pdbg.h'
INC_DIR += $(REP_DIR)/src/lib/libc

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(LIBC_GEN_DIR)
