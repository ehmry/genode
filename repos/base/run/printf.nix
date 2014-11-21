{ run, pkgs }:

with pkgs;

run {
  name = "printf";

  contents = [
    { target = "/"; source = test.printf; }
    { target = "/config";
      source = builtins.toFile "config" ''
	<config>
		<parent-provides>
			<service name="LOG"/>
			<service name="RM"/>
		</parent-provides>
		<default-route>
			<any-service> <parent/> </any-service>
		</default-route>
		<start name="test-printf">
			<resource name="RAM" quantum="10M"/>
		</start>
	</config>
      '';
    }
  ];

  testScript =
    ''
      append qemu_args "-nographic -m 64"

      run_genode_until {-1 = -1 = -1} 10

      puts "Test succeeded"
    '';
}
