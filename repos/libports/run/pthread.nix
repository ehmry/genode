{ run, pkgs }:

with pkgs;

run {
  name = "pthread";

  contents = [
    { target = "/"; source = test.pthread; }
    { target = "/"; source = libs.pthread; }
    { target = "/"; source = libs.libc; }
    { target = "/config";
      source = builtins.toFile "config" ''
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
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<start name="test-pthread">
		<resource name="RAM" quantum="2M"/>
		<config>
			<libc stdout="/dev/log">
				<vfs> <dir name="dev"> <log/> </dir> </vfs>
			</libc>
		</config>
	</start>
        </config>
      '';
    }
  ];

  testScript =
    ''
      append qemu_args " -nographic -m 64 "

      run_genode_until "--- returning from main ---" 10

      puts "Test succeeded"
    '';
}
