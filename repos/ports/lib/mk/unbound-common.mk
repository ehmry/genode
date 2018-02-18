LIBS += libc libssl

UNBOUND_PORT_DIR := $(call select_from_ports,unbound)
UNBOUND_SRC_DIR := $(UNBOUND_PORT_DIR)/src/app/unbound

INC_DIR += $(UNBOUND_SRC_DIR)

CC_DEF += -DRETSIGTYPE=void

SRC_C_COMMON = \
ervices/cache/dns.c services/cache/infra.c services/cache/rrset.c \
util/as112.c util/data/dname.c util/data/msgencode.c util/data/msgparse.c \
util/data/msgreply.c util/data/packed_rrset.c iterator/iterator.c \
iterator/iter_delegpt.c iterator/iter_donotq.c iterator/iter_fwd.c \
iterator/iter_hints.c iterator/iter_priv.c iterator/iter_resptype.c \
iterator/iter_scrub.c iterator/iter_utils.c services/listen_dnsport.c \
services/localzone.c services/mesh.c services/modstack.c services/view.c \
services/outbound_list.c services/outside_network.c util/alloc.c \
util/config_file.c util/configlexer.c util/configparser.c \
util/shm_side/shm_main.c services/authzone.c\
util/fptr_wlist.c util/locks.c util/log.c util/mini_event.c util/module.c \
util/netevent.c util/net_help.c util/random.c util/rbtree.c util/regional.c \
util/rtt.c util/storage/dnstree.c util/storage/lookup3.c \
util/storage/lruhash.c util/storage/slabhash.c util/timehist.c util/tube.c \
util/ub_event.c \
validator/autotrust.c validator/val_anchor.c validator/validator.c \
validator/val_kcache.c validator/val_kentry.c validator/val_neg.c \
validator/val_nsec3.c validator/val_nsec.c validator/val_secalgo.c \
validator/val_sigcrypt.c validator/val_utils.c dns64/dns64.c \
edns-subnet/edns-subnet.c edns-subnet/subnetmod.c \
edns-subnet/addrtree.c edns-subnet/subnet-whitelist.c \
cachedb/cachedb.c respip/respip.c

SRC_C += $(notdir $(SRC_C_COMMON)) genode_reallocarray.c genode_signal.c

SRC_C_COMMON_DIRS := $(addprefix $(UNBOUND_SRC_DIR)/,$(sort $(dir $(SRC_C_COMMON))))

vpath %.c $(REP_DIR)/src/lib/unbound $(SRC_C_COMMON_DIRS)
