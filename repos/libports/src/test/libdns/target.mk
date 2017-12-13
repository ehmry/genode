TARGET  = test-libdns
CC_DEF += -DDNS_MAIN
SRC_C   = dns.c
LIBS    = posix

vpath %.c /home/emery/repo/libdns/src
