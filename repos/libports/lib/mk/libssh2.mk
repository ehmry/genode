LIBSSH2_PORT_DIR := $(call select_from_ports,libssh2)

SRC_C += \
	channel.c comp.c crypt.c hostkey.c kex.c mac.c misc.c \
	packet.c publickey.c scp.c session.c sftp.c userauth.c transport.c \
	version.c knownhost.c agent.c $(CRYPTO_CSOURCES) pem.c keepalive.c global.c

SRC_C += openssl.c

INC_DIR += $(LIBSSH2_PORT_DIR)/include
INC_DIR += $(REP_DIR)/src/lib/libssh2

LIBS += libc libcrypto

SHARED_LIB = yes

vpath %.c $(LIBSSH2_PORT_DIR)/src/lib/libssh2/src

CC_CXX_WARN_STRICT =
