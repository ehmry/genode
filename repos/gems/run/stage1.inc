source ${genode_dir}/repos/base/run/platform_drv.inc

append build_components {
	core init
	drivers/ahci
	drivers/timer
	drivers/usb
	lib/vfs/rump
	server/fs_rom
	server/part_blk
	server/report_rom
	server/rom_blk
	server/rom_fallback
	server/blk_router
	server/vfs
}

append build_components { } $stage2_build_components
append_platform_drv_build_components

build $build_components

create_boot_directory

append config {
<config>
	<affinity-space width="2" height="2"/>
	<parent-provides>
		<service name="CPU"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="IRQ"/>
		<service name="LOG"/>
		<service name="PD"/>
		<service name="RAM"/>
		<service name="RM"/>
		<service name="ROM"/>
		<service name="TRACE"/>
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>

	<start name="timer">
		<resource name="RAM" quantum="2M"/>
		<provides> <service name="Timer"/> </provides>
	</start>

	<start name="acpi_drv" >
		<resource name="RAM" quantum="3M"/>
		<route>
			<service name="Report"> <child name="acpi_report_rom"/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<start name="acpi_report_rom"  >
		<binary name="report_rom"/>
		<resource name="RAM" quantum="2M"/>
		<provides>
			<service name="ROM" />
			<service name="Report" />
		</provides>
		<config>
			<policy label="platform_drv -> acpi" report="acpi_drv -> acpi"/>
		</config>
		<route> <any-service> <parent/> </any-service> </route>
	</start>

	<start name="platform_drv" >
		<resource name="RAM" quantum="4M" constrain_phys="yes"/>
		<provides>
			<service name="Platform"/>
			<service name="Acpi"/>
		</provides>
		<route>
			<service name="ROM" label="system"> <child name="acpi_report_rom"/> </service>
			<service name="ROM" label="acpi">   <child name="acpi_report_rom"/> </service>
			<any-service> <parent/> </any-service>
		</route>
		<config acpi="yes">
			<policy label_prefix="stage2 -> ps2_drv">  <device name="PS2"/> </policy>
			<policy label_prefix="stage2 -> nic_drv">  <pci class="ETHERNET"/> </policy>
			<policy label_prefix="stage2 -> fb_drv">   <pci class="VGA"/> </policy>
			<policy label_prefix="stage2 -> wifi_drv"> <pci class="WIFI"/> </policy>
			<policy label_prefix="usb_drv">  <pci class="USB"/> </policy>
			<policy label_prefix="ahci_drv"> <pci class="AHCI"/> </policy>
			<policy label_prefix="stage2 -> audio_drv"> <pci class="AUDIO"/> <pci class="HDAUDIO"/> </policy>
			<policy label_prefix="stage2 -> intel_fb_drv" irq_mode="nomsi">
				<pci class="VGA"/>
				<pci bus="0" device="0" function="0"/>
				<pci class="ISABRIDGE"/>
			</policy>
		</config>
	</start>

	<start name="ahci_drv" >
		<resource name="RAM" quantum="8M" />
		<provides> <service name="Block" /> </provides>
		<config>
			<policy label_prefix="ahci_part_blk" device="0" />
		</config>
	</start>

	<start name="usb_drv">
		<resource name="RAM" quantum="16M"/>
		<provides>
			<service name="Block" />
			<service name="Input"/>
		</provides>
		<config uhci="yes" ehci="yes" xhci="yes">
			<hid/>
			<storage/>
		</config>
	</start>

	<start name="ahci_part_blk">
		<binary name="part_blk"/>
		<resource name="RAM" quantum="4M" />
		<provides><service name="Block" /></provides>
		<route>
			<service name="Block">
				<child name="ahci_drv"/> </service>
			<service name="Report">
				<child name="blk_router"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
		<config report="yes">
			<policy label_prefix="blk_router ->" client_select="yes"/>
		</config>
	</start>

	<start name="usb_part_blk">
		<binary name="part_blk"/>
		<resource name="RAM" quantum="4M" />
		<provides><service name="Block" /></provides>
		<route>
			<service name="Block">
				<child name="usb_drv"/> </service>
			<service name="Report">
				<child name="blk_router"/> </service>
			<any-service><parent/><any-child/></any-service>
		</route>
		<config report="yes">
			<policy label_prefix="blk_router ->" client_select="yes"/>
		</config>
	</start>

	<start name="blk_router">
		<resource name="RAM" quantum="2M" />
		<provides>
			<service name="Report"/>
			<service name="Block"/>
		</provides>
		<config>
			<policy label_prefix="vfs">
				<partition type="63" />
				<partition type="64" timeout="1"/>
			</policy>
			<policy label_prefix="stage2">
				<partition type="83" />
			</policy>
		</config>
		<route>
			<service name="Block" label_prefix="ahci_part_blk ->">
				<child name="ahci_part_blk"/> </service>
			<service name="Block" label_prefix="usb_part_blk ->">
				<child name="usb_part_blk"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="vfs">
		<resource name="RAM" quantum="10M"/>
		<provides><service name="File_system"/></provides>
		<config>
			<vfs> <rump fs="ext2fs" writeable="true"/> </vfs>
			<policy label_prefix="fs_rom" root="/genode"/>
			<policy label_prefix="stage2 -> chroot" root="/chroot" writeable="yes"/>
			<default-policy writeable="yes"/>
		</config>
		<route>
			<service name="Block">
				<child name="blk_router"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="fs_rom">
		<resource name="RAM" quantum="256M"/>
		<provides><service name="ROM"/></provides>
		<route>
			<service name="File_system">
				<child name="vfs"/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<!-- serve ROMs from kernel memory when available -->
	<start name="rom_fallback">
		<resource name="RAM" quantum="4M"/>
		<provides> <service name="ROM"/> </provides>
		<config>
			<fallback/>
			<fallback label="fs"/>
		</config>
		<route>
			<service name="ROM" label_prefix="fs ->" >
				<child name="fs_rom"/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<start name="stage2">
		<binary name="init"/>
		<resource name="RAM" quantum="3G"/>
		<provides><service name="ROM"/></provides>
		<configfile name="stage2.config"/>
		<route>
			<service name="ROM">
				<child name="rom_fallback"/> </service>
			<service name="Block">
				<child name="blk_router"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

</config>}

install_config $config

append boot_modules {
	core init ld.lib.so
	ahci_drv
	acpi_drv
	blk_router
	device_pd
	fs_rom
	part_blk
	platform_drv
	report_rom
	rom_fallback
	rump_fs.lib.so
	rump.lib.so
	timer
	usb_drv
	vfs
	vfs_rump.lib.so
}

if {[info exists stage2_config]} {
	set stage2_fd [open "bin/stage2.config" w]
	puts $stage2_fd $stage2_config
	close $stage2_fd

	lappend boot_modules stage2.config
}

append boot_modules $stage2_boot_modules
append_platform_drv_boot_modules

build_boot_image $boot_modules

run_genode_until forever
