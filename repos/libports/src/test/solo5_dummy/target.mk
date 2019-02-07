TARGET = unikernel.solo5
LIBS  += solo5
SRC_C += unikernel.c

INC_DIR += $(call select_from_ports,solo5)/src/lib/solo5
