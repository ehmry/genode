content: src/noux-pkg/pkgconf LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/pkgconf)

src/noux-pkg/pkgconf:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/noux-pkg/pkgconf/* $@
	cp -a  $(REP_DIR)/src/noux-pkg/pkgconf/* $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/pkgconf/COPYING $@
