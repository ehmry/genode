#
# \brief  Test thread-local errno works for hybrid Linux/Genode programs
# \author Norman Feske
# \date   2011-12-05
#

{ run, pkgs }:

with pkgs;

run {
  name = "lx_hybrid_errno";

  contents =
    [ { target = "/"; source = test.lx_hybrid_errno; }
      { target = "/config";
        source = builtins.toFile "config"
          ''
	<config>
		<parent-provides>
			<service name="ROM"/>
			<service name="LOG"/>
		</parent-provides>
		<default-route>
			<any-service> <parent/> <any-child/> </any-service>
		</default-route>
		<start name="test-lx_hybrid_errno">
			<resource name="RAM" quantum="1M"/>
		</start>
	</config>
           '';
      }
    ];

  testScript =
    ''
      run_genode_until "--- finished thread-local errno test ---.*\n" 10
    '';
}
