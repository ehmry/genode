#
# \brief  Test if the exception mechanism works in hybrid applications
# \author Christian Prochaska
# \date   2011-11-22
#

{ run, pkgs }:

with pkgs;

run {
  name = "lx_hybrid_exception";

  contents =
    [ { target = "/"; source = test.lx_hybrid_exception; }
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
		<start name="test-lx_hybrid_exception">
			<resource name="RAM" quantum="1M"/>
		</start>
	</config>
          '';
      }
    ];

  testScript =
    ''
      run_genode_until {child "test-lx_hybrid_exception" exited with exit value 0.*\n} 10
    '';
}
