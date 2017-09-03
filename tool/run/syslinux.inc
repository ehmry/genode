##
# Generate Syslinux multiboot config file
#
proc create_syslinux_cfg {args} {

	exec mkdir -p [run_dir]/boot/syslinux
	set fh [open "[run_dir]/boot/syslinux/syslinux.cfg" "WRONLY CREAT TRUNC"]
	puts $fh "SERIAL 0 115200"
	puts $fh "DEFAULT genode"
	puts $fh "LABEL genode"
	puts $fh " SAY Booting Genode..."
	puts $fh " KERNEL /boot/syslinux/mboot.c32"

	set mboot_line " APPEND "
	append mboot_line [join $args " --- "]
	puts $fh $mboot_line

	close $fh
}
