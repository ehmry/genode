CAIRO_PORT_DIR := $(call select_from_ports,cairo)
CAIRO_SRC_DIR := $(CAIRO_PORT_DIR)/src/lib/cairo/src

INC_DIR += \
	$(call select_from_repositories,/include/cairo) \
	$(CAIRO_PORT_DIR)/include/cairo \
