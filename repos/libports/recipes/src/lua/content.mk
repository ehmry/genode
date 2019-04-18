PORT_DIR = $(call port_dir,$(GENODE_DIR)/repos/libports/ports/lua)

content: src/lib/lua

src/lib/lua:
	mkdir -p $@
	cp -r $(PORT_DIR)/$@/* $@/

MIRROR_FROM_REP_DIR := \
	src/app/lua \
	lib/mk/luacxx.mk \
	lib/mk/lua.inc \
	lib/mk/lua.mk \
	lib/import/import-luacxx.mk \
	lib/import/import-lua.mk \

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/lua/COPYRIGHT $@
