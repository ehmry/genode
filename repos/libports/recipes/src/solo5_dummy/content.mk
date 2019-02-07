SRC_DIR = src/test/solo5_dummy
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: src/lib/solo5/bindings

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/solo5)

src/lib/solo5/bindings: $(PORT_DIR)
	mkdir -p $(dir $@)
	cp -r $</$@ $@
