{ runtime, pkgs, ... }:

runtime.test rec {
  name = "ninep_client";
  automatic = false;

  components = with pkgs;
    [ drivers.timer
      drivers.nic.dde_ipxe drivers.pci drivers.pci.device_pd
      server.ninep_client
      test.libc_vfs
    ];

  config = builtins.toFile "config.xml"
    ''
<config verbose="yes">
	<parent-provides>
		<service name="ROM"/>
		<service name="RAM"/>
		<service name="IRQ"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="CAP"/>
		<service name="PD"/>
		<service name="RM"/>
		<service name="CPU"/>
		<service name="LOG"/>
		<service name="SIGNAL"/>
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>

	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Timer"/> </provides>
	</start>

	<start name="nic_drv">
		<resource name="RAM" quantum="4M"/>
		<provides><service name="Nic"/></provides>
	</start>

	<start name="pci_drv">
		<resource name="RAM" quantum="4M" constrain_phys="yes"/>
		<provides> <service name="PCI"/> </provides>
	</start>

	<start name="9p_client">
		<resource name="RAM" quantum="8M"/>
		<provides><service name="File_system"/></provides>
		<config addr="10.0.2.2">
			<policy label="client1" root="/tmp/genode/client1" writeable="yes" user="nobody"/>
			<policy label="client2" root="/tmp/genode/client2" writeable="yes" user="nobody"/>
		</config>
	</start>

	<start name="client1">
		<binary name="test-libc_vfs"/>
		<resource name="RAM" quantum="2M"/>
		<config>
			<iterations value="2"/>
			<libc stdout="/dev/log" cwd="/tmp" >
				<vfs>
					<dir name="tmp" ><fs/></dir>
					<dir name="dev"> <log/> </dir>
				</vfs>
			</libc>
		</config>
	</start>
	<start name="client2">
		<binary name="test-libc_vfs"/>
		<resource name="RAM" quantum="2M"/>
		<config>
			<iterations value="1"/>
			<libc stdout="/dev/log" cwd="/tmp" >
				<vfs>
					<dir name="tmp" ><fs/></dir>
					<dir name="dev"> <log/> </dir>
				</vfs>
			</libc>
		</config>
	</start>
</config>
    '';

  # # Use the following to dump the traffic.
  # -net dump,file=/tmp/${name}.pcap

  testScript =
    ''
      file mkdir /tmp/genode/client1 /tmp/genode/client2
      append qemu_args " -m 512 -net nic,model=e1000 -net user -net dump,file=/tmp/${name}.pcap"
      run_genode_until {child "client1" exited with exit value 0} 60
      file delete -force /tmp/genode/client1 /tmp/genode/client2
    '';
}