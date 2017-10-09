TARGET   = test-ping_server
LIBS     = posix
SRC_CC   = main.cc pingpong.cc

vpath pingpong.cc $(PRG_DIR)/..
