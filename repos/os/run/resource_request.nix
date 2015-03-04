{ runtime, pkgs, ... }:

runtime.test {
  name = "resource_request";

  components = with pkgs;
    [ drivers.timer test.resource_request ];

  
    config = builtins.toFile "config.xml"
      ''
	<config>
		<parent-provides>
			<service name="ROM"/>
			<service name="RAM"/>
			<service name="CPU"/>
			<service name="RM"/>
			<service name="CAP"/>
			<service name="PD"/>
			<service name="IRQ"/>
			<service name="IO_PORT"/>
			<service name="IO_MEM"/>
			<service name="SIGNAL"/>
			<service name="LOG"/>
		</parent-provides>
		<default-route>
			<any-service> <parent/> <any-child/> </any-service>
		</default-route>
		<start name="timer">
			<resource name="RAM" quantum="1M"/>
			<provides><service name="Timer"/></provides>
		</start>
		<start name="test-resource_request">
			<resource name="RAM" quantum="2M"/>
			<provides> <service name="ROM" /> </provides>
		</start>
	</config>
      '';


  testScript =
    ''
      append qemu_args "-nographic -m 128"
      run_genode_until {child "test-resource_request" exited with exit value 0.*\n} 30
    '';
}
