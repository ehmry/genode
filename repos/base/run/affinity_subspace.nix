#
# \brief  Test to affinity subspacing
# \author Norman Feske
#

{ spec, run, pkgs }:

#if {[have_spec platform_pbxa9] || (![have_spec nova] && ![have_spec foc])} {
#	puts "Platform is unsupported."
#	exit 0
#}

if spec.kernel != "nova" then null else

run {
  name = "affinity_subspace";

  contents = [
    { target = "/"; source = pkgs.test.affinity; }
    { target = "/config";
      source = builtins.toFile "config" ''
	<config>
		<parent-provides>
			<service name="LOG"/>
			<service name="CPU"/>
			<service name="RM"/>
			<service name="ROM"/>
			<service name="RAM"/>
			<service name="CAP"/>
			<service name="PD"/>
			<service name="SIGNAL"/>
		</parent-provides>
		<affinity-space width="2" />
		<default-route>
			<any-service> <parent/> </any-service>
		</default-route>
		<start name="init">
			<resource name="RAM" quantum="10M"/>
			<!-- assign the right half of the available CPUs -->
			<affinity xpos="1" width="1" />
			<config>
				<parent-provides>
					<service name="LOG"/>
					<service name="CPU"/>
					<service name="RM"/>
				</parent-provides>
				<default-route>
					<any-service> <parent/> </any-service>
				</default-route>
				<!-- assign the leftmost half of CPUs to test-affinity -->
				<affinity-space width="2" />
				<start name="test-affinity">
					<resource name="RAM" quantum="2M"/>
					<affinity xpos="0" width="1" />
				</start>
			</config>
		</start>
	</config>
      '';
    }
  ];

  testScript =
    ''
      append qemu_args " -nographic -m 64 -smp 8,cores=8 "

      run_genode_until {.*Detected 2x1 CPUs.*} 60

      puts "Test succeeded"
    '';
}
