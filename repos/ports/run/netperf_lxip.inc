append build_components { lib/vfs/lxip }
append boot_modules { lxip.lib.so vfs_lxip.lib.so }

set vfs_plugin_name lxip
set ip_match_string "ipaddr=(\[0-9\]+\.\[0-9\]+\.\[0-9\]+\.\[0-9\]+).*\n"
