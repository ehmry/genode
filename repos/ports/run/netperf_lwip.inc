append build_components { lib/vfs/lwip }
append boot_modules { vfs_lwip.lib.so }

set vfs_plugin_name     lwip
set ip_match_string "address=(\[0-9\]+\.\[0-9\]+\.\[0-9\]+\.\[0-9\]+).*\n"
