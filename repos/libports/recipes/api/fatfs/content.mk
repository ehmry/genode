PORT_DIR := $(call port_dir,$(REP_DIR)/ports/fatfs)

MIRROR_FROM_PORT_DIR := include src
MIRROR_FROM_REP_DIR := \
	include/fatfs/block.h \
	lib/import/import-fatfs_block.mk \
	lib/mk/fatfs_block.mk \
	src/lib/fatfs/diskio_block.cc \

content: $(MIRROR_FROM_PORT_DIR) $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $@
