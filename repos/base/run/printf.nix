{ runtime, pkgs }:

runtime.test rec {
  name = "printf";

  components = [ pkgs.test.printf ];

  testScript =
    ''
      run_genode_until {-1 = -1 = -1} 10
    '';
}
