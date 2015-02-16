/*
 * \brief  Basic test for genode timer-session
 * \author Martin Stein
 * \date   2012-05-29
 */

{ run, pkgs, runtime }:

with pkgs;

run rec {
  name = "timer";

  contents = [
    { target = "/"; source = drivers.timer; }
    { target = "/"; source = test.timer; }
    { target = "/config";
      source = runtime.mkInitConfig {
        inherit name; components = [ drivers.timer test.timer ];
      };
    }
  ];

  testScript =
    ''
      run_genode_until "--- timer test finished ---" 60
    '';

}
