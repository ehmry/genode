#
# \brief  Test IPC from pthread created outside of Genode
# \author Norman Feske
# \date   2011-12-20
#

{ run, pkgs }:

with pkgs;

run {
  name = "lx_hybrid_pthread_ipc";

  contents =
    [ { target = "/"; source = test.lx_hybrid_pthread_ipc; }
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
		<start name="test-lx_hybrid_pthread_ipc">
			<resource name="RAM" quantum="1M"/>
		</start>
	</config>
          '';
      }
    ];

  testScript =
    ''
      run_genode_until "--- finished pthread IPC test ---.*\n" 10
    '';
}
