#
# \brief  IOzone VFS benchmarking
# \author Emery Hemingway
# \date   2017-07-25
#

#
# \arg test_name
# \arg use_blk_cache
# \arg mkfs_cmd
# \arg mkfs_opts
# \arg test_build_components
# \arg test_vfs_config
# \arg test_boot_modules
# \arg max_record_size
#

if {[have_spec odroid_xu] || [have_spec zynq]} {
	puts "Run script does not support this platform."
	exit 0
}

# use SD on ARM
set use_sd_card_drv [expr [have_spec omap4] || [have_spec arndale] || [have_spec pl180]]

# use AHCI on x86
set use_ahci        [expr [have_spec x86] && ![have_spec linux]]

# use ram_blk on Linux
set use_ram_blk     [have_spec linux]

if {[catch { exec which $mkfs_cmd } ]} {
	puts stderr "Error: $mkfs_cmd not installed, aborting test"; exit }

#if {[expr ![have_include "power_on/qemu"] && !$use_ram_blk]} {
#	puts "\nPlease setup your native sd or hard drive. Remove this fail stop";
#	puts "check when you have prepared your native environment.\n";
#	exit 0
#}

source ${genode_dir}/repos/base/run/platform_drv.inc

#
# Build
#

set build_components { app/iozone }

lappend build_components {*}$test_build_components

lappend_if $use_sd_card_drv build_components drivers/sd_card

lappend_if $use_blk_cache build_components server/blk_cache

append_platform_drv_build_components

build $build_components

create_boot_directory

set depot_pkgs "genodelabs/src/[base_src] genodelabs/src/init"

lappend_if $use_ahci       depot_pkgs genodelabs/src/ahci_drv
lappend_if $use_ram_blk    depot_pkgs genodelabs/src/ram_blk

import_from_depot {*}$depot_pkgs


#
# Generate config
#

set config {
<config>
	<parent-provides>
		<service name="CPU"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="IRQ"/>
		<service name="LOG"/>
		<service name="PD"/>
		<service name="RM"/>
		<service name="ROM"/>
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<default caps="128"/>
	<start name="timer" caps="64">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Timer"/> </provides>
	</start>
	<start name="iozone" caps="1024">
		<resource name="RAM" quantum="3G"/>
		<config>
			<vfs>}

append config $test_vfs_config

append config {
				<dir name="dev">
					<log/>
					<inline name="rtc">2000-01-01 00:00</inline>
				</dir>
			</vfs>
			<libc stdout="/dev/log" stderr="/dev/log" rtc="/dev/rtc"/>
			<arg value="iozone"/>
			<arg value="-a"/>
}
append config  "<arg value=\"-g\"/> <arg value=\"$max_record_size\"/>"
append config {
			<arg value="-i"/> <arg value="0"/>
			<arg value="-i"/> <arg value="1"/>
			<arg value="-i"/> <arg value="2"/>
			<arg value="-i"/> <arg value="3"/>
			<arg value="-i"/> <arg value="4"/>
			<arg value="-i"/> <arg value="5"/>
			<arg value="-i"/> <arg value="6"/>
			<arg value="-i"/> <arg value="7"/>
			<arg value="-i"/> <arg value="8"/>
		</config>
		<route>
			<service name="Block">}
if {$use_blk_cache} {
	append config {<child name="blk_cache"/>}
} else {
	append config {<child name="blk_drv"/>}
}

append config {
			</service>
			<any-service>
				<parent/> <any-child/>
			</any-service>
		</route>
	</start>}

append_platform_drv_config

append_if $use_ahci config {
	<start name="ahci_drv">
		<resource name="RAM" quantum="10M" />
		<provides><service name="Block" /></provides>
		<route>
			<any-service> <parent /> <any-child /></any-service>
		</route>
		<config>
			<default-policy device="0" />
		</config>
	</start>
	<alias name="blk_drv" child="ahci_drv"/>}


append_if $use_sd_card_drv config {
	<start name="sd_card_drv">
		<resource name="RAM" quantum="1M" />
		<provides><service name="Block"/></provides>
	</start>
}

append_if $use_blk_cache config {
	<start name="blk_cache">
		<resource name="RAM" quantum="256M" />
		<provides><service name="Block"/></provides>
		<route>
			<service name="Block">
				<child name="blk_drv"/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start>}

append_if $use_ram_blk config {
	<start name="ram_blk">
		<resource name="RAM" quantum="128M" />
		<provides><service name="Block"/></provides>
		<config file="test.hda" block_size="512"/>
	</start>
	<alias name="blk_drv" child="ram_blk"/>}

append config {
</config>
}

install_config $config

#
# Boot modules
#

append boot_modules { libc.lib.so libm.lib.so posix.lib.so vfs.lib.so iozone }

lappend boot_modules {*}$test_boot_modules

lappend_if $use_sd_card_drv  boot_modules sd_card_drv
lappend_if $use_blk_cache    boot_modules blk_cache
lappend_if $use_ram_blk      boot_modules test.hda

append_platform_drv_boot_modules

build_boot_image $boot_modules


#
# Execute test case
#

set disk_image "bin/test.hda"
set cmd "dd if=/dev/zero of=$disk_image bs=1024 count=98304"
puts "creating disk image: $cmd"
catch { exec sh -c $cmd }

set cmd "$mkfs_cmd $mkfs_opts $disk_image"
puts "formating disk: $cmd"
catch { exec sh -c $cmd }


#
# Qemu
#

append qemu_args "  -nographic "
append_if $use_ahci        qemu_args " -drive id=disk,file=$disk_image,format=raw,if=none -device ahci,id=ahci -device ide-drive,drive=disk,bus=ahci.0 -boot d"
append_if $use_sd_card_drv qemu_args " -drive file=$disk_image,format=raw,if=sd,cache=writeback "


run_genode_until {child "iozone" exited.*\n} 1200

regexp ".init -> iozone. .*\n" $output iozone_output

set iozone_output [ string map {"\[init -> iozone\] " ""} $iozone_output ]

set datadir var/data/iozone
file mkdir $datadir
set datafile $datadir/$test_name.out

set fd [open $datafile w]
puts $fd $iozone_output
close $fd

puts "\nIOzone data written to $datafile"
