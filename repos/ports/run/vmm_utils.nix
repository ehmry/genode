{ spec, run, pkgs }:

with pkgs;

if spec.isNOVA then run {
  name = "vmm_utils";

  contents = [
    { target = "/"; source = drivers.timer; }
    { target = "/"; source = test.vmm_utils; }
    { target = "/config";
      source = builtins.toFile "config"
        ''
<config>
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
		<any-service><parent/><any-child/></any-service>
	</default-route>
	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Timer"/></provides>
	</start>
	<start name="test-vmm_utils">
		<resource name="RAM" quantum="1G"/>
	</start>
</config>
      '';
    }
  ];

  testScript =
    ''
      append qemu_args " -m 512 "
      append qemu_args " -cpu phenom "
      append qemu_args " -nographic "

      run_genode_until {.*VMM: _svm_startup called} 30
    '';
} else null
