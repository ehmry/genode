TARGET = test-glib
LIBS = libc pthread glib posix
SRC_C = main.c

CC_DEF -Dgintptr=int

# Strip the required one-time-use missing type from the source
main.c: $(call select_from_ports,glib)/src/lib/glib/tests/testglib.c
	$(VERBOSE)sed 's/gintptr/int/' $< > $@
