{ spec, run, pkgs }:

if spec.kernel == "linux" then null else

run {
  name = "cap_integrity";

  contents = [
    { target = "/"; source = pkgs.test.cap_integrity; }
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
			<service name="SIGNAL"/>
			<service name="LOG"/>
		</parent-provides>
		<default-route>
			<any-service> <parent/> </any-service>
		</default-route>
		<start name="test-cap_integrity">
			<resource name="RAM" quantum="10M"/>
		</start>
	</config>
      '';
    }
  ];

  testScript = ''
append qemu_args "-nographic -m 64"

# increase expect buffer size, since there might be many log messages
match_max -d 100000

run_genode_until {child exited with exit value 0.*} 60

grep_output {\[init\] test message}
compare_output_to { }
'';
}
