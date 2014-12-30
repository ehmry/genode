#
# \brief  Test if global static constructors in hybrid applications and
#         host shared libs get called
# \author Christian Prochaska
# \date   2011-11-24
#

{ run, pkgs }:

with pkgs;

run {
  name = "lx_hybrid_ctors";

  contents =
    [ { target = "/"; source = test.lx_hybrid_ctors; }
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
		<start name="test-lx_hybrid_ctors">
			<resource name="RAM" quantum="1M"/>
		</start>
	</config>
          '';
      }
    ];

    testScript =
      ''
        #
        # Execute test case
        #

        run_genode_until {child "test-lx_hybrid_ctors" exited with exit value 0.*\n} 10

        #
        # Compare output
        #

        grep_output {\[init -\> test-lx_hybrid_ctors\]}

        compare_output_to {
        	[init -> test-lx_hybrid_ctors] Global static constructor of host library called.
                [init -> test-lx_hybrid_ctors] Global static constructor of Genode application called
                [init -> test-lx_hybrid_ctors] --- lx_hybrid global static constructor test ---
                [init -> test-lx_hybrid_ctors] --- returning from main ---
        }
      '';
}
