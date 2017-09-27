TARGET   = test-ping_client
LIBS     = posix
SRC_CC   = main.cc pingpong.cc

vpath pingpong.cc $(PRG_DIR)/..
