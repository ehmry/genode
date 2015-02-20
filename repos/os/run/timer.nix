/*
 * \brief  Basic test for genode timer-session
 * \author Martin Stein
 * \date   2012-05-29
 */

{ runtime, pkgs, ... }:

runtime.test {
  name = "timer";

  components = with pkgs;
    [ drivers.timer test.timer ];

  testScript = "run_genode_until \"--- timer test finished ---\" 60";
}
