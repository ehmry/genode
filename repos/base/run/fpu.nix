{ run, pkgs }:

with pkgs;

run {
  name = "fpu";

  contents = [
    { target = "/"; source = test.fpu; }
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
			<service name="SIGNAL"/>
		</parent-provides>
		<default-route>
			<any-service> <parent/> </any-service>
		</default-route>
		<start name="test">
			<binary name="test-fpu"/>
			<resource name="RAM" quantum="10M"/>
		</start>
	</config>
      '';
    }
  ];

  testScript = ''
    append qemu_args "-nographic -m 64"

    run_genode_until "test done.*\n" 60

    grep_output {^\[init -\> test\]}

    #compare_output_to {
    # 	[init -> test] FPU user started
    #	[init -> test] FPU user started
    #	[init -> test] FPU user started
    #	[init -> test] FPU user started
    #	[init -> test] FPU user started
    #	[init -> test] FPU user started
    #	[init -> test] FPU user started
    #	[init -> test] FPU user started
    #	[init -> test] FPU user started
    #	[init -> test] FPU user started
    #	[init -> test] test done
    #}
  '';

}
