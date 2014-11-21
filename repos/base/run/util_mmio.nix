{ run, pkgs }:

run {
  name = "util_mmio";

  contents = [
    { target = "/"; source = pkgs.test.util_mmio; }
    { target = "/config";
      source = builtins.toFile "config" ''
	<config>
		<parent-provides>
			<service name="ROM"/>
			<service name="RAM"/>
			<service name="CPU"/>
			<service name="RM"/>
			<service name="CAP"/>
			<service name="PD"/>
			<service name="LOG"/>
		</parent-provides>
		<default-route>
			<any-service> <parent/> </any-service>
		</default-route>
		<start name="test">
			<binary name="test-util_mmio"/>
			<resource name="RAM" quantum="10M"/>
		</start>
	</config>
      '';
    }
  ];

  testScript =
    ''
      #
      # Execution
      #

      append qemu_args "-nographic -m 64"

      run_genode_until "Test done.*\n" 10

      #
      # Conclusion
      #

      grep_output {\[init -\> test\]}

      compare_output_to {
      	[init -> test] Test done
      }
    '';
}
