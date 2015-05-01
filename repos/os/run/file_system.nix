/*
 * \brief  Basic test for File_system sessions
 * \author Emery Hemingway
 * \date   2015-04-30
 */

{ runtime, pkgs, ... }:

runtime.test {
  name = "file_system";
  automatic = false;

  components = with pkgs;
    [ drivers.timer test.file_system
      #server.ram_fs
      drivers.nic.dde_ipxe
      drivers.pci drivers.pci.device_pd
      server.ninep_fs
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

	<start name="ram_fs">
		<resource name="RAM" quantum="8M"/>
		<provides><service name="File_system"/></provides>
		<config>
			<policy label="" root="/" writeable="yes"/>
		</config>
	</start>

	<start name="9p_fs">
		<resource name="RAM" quantum="8M"/>
		<provides><service name="File_system"/></provides>
		<config addr="10.0.2.2">
			<policy label="" root="/tmp" writeable="yes" user="nobody"/>
		</config>
	</start>

	<start name="test-file_system">
		<resource name="RAM" quantum="4M"/>
		<config iterations="64"/>
	</start>
</config>
  '';

  testScript = "run_genode_until \"--- File_system test finished ---\" 60";
}
