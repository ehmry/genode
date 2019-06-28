SRC_DIR = src/server/stereo_patch
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content:
	mkdir -p src/server/mixer
	cp $(REP_DIR)/src/server/cached_fs_rom/session_requests.h $(SRC_DIR)
