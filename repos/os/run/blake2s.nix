{ runtime, pkgs }:

with pkgs;

runtime.test rec {
  name = "blake2s";

  components = [ test.blake2s ];

  testScript =
    ''
      run_genode_until {child "test-blake2s" exited with exit value 0} 20
    '';
}
