TARGET := nix-instantiate

include $(REP_DIR)/src/app/nix/nix-common.inc

LIBS += libc libnixexpr libnixmain libnixstore libnixutil libnixformat

INC_DIR += $(NIX_DIR)/src/libexpr  # eval.hh
INC_DIR += $(NIX_DIR)/src/libmain  # shared.hh
INC_DIR += $(NIX_DIR)/src/libstore # globals.hh

SRC_CC := nix-instantiate.cc

vpath %.cc $(NIX_DIR)/src/nix-instantiate
