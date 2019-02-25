#
# Template for testing non-interactive Noux ports not requiring fork
#

build $build_components

import_from_depot [depot_user]/src/[base_src] \
                  [depot_user]/src/init \
                  [depot_user]/src/libc \
                  [depot_user]/src/log_terminal \
                  [depot_user]/src/noux \
                  [depot_user]/src/posix \
                  [depot_user]/src/vfs \

install_config {
	<config verbose="yes">
		<parent-provides>
			<service name="ROM"/>
			<service name="LOG"/>
			<service name="RM"/>
			<service name="CPU"/>
			<service name="PD"/>
			<service name="IRQ"/>
			<service name="IO_MEM"/>
			<service name="IO_PORT"/>
		</parent-provides>
		<default-route>
			<any-service> <any-child/> <parent/> </any-service>
		</default-route>
		<default caps="100"/>
		<start name="timer">
			<resource name="RAM" quantum="1M"/>
			<provides><service name="Timer"/></provides>
		</start>
		<start name="terminal">
			<binary name="log_terminal" />
			<resource name="RAM" quantum="1M"/>
			<provides><service name="Terminal"/></provides>
		</start>
} $app_config {
</config>}

build_boot_image $boot_modules

append qemu_args " -nographic -serial mon:stdio "

run_genode_until {child "noux" exited with exit value 0.*\n} 600
