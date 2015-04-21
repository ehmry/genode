{ runtime, pkgs }:

with pkgs;

runtime.test rec {
  name = "sha256";

  components = [ test.sha256 ];

  testScript =
    ''
      run_genode_until {child "test-sha256" exited with exit value 0} 20
    '';
}
