TARGET  = lua
LIBS   += lua libc posix

LUA_DIR = $(call select_from_ports,lua)/src/lib/lua

SRC_C += lua.c

vpath %.c $(LUA_DIR)/src
