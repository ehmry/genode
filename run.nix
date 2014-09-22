/*
 * \brief  Entry expression for runs
 * \author Emery Hemingway
 * \date   2014-09-16
 */

{ system, nixpkgs, pkgs }:

let
  importRun = p: import p { inherit run pkgs; };

  linuxRun = import ./repos/base-linux/tool/run { inherit nixpkgs pkgs; };
  novaRun  = import ./repos/base-nova/tool/run { inherit nixpkgs pkgs; };

  run =
      if system == "x86_32-linux" then linuxRun else
      if system == "x86_64-linux" then linuxRun else
      #if system == "x86_32-nova" then novaRun else
      #if system == "x86_64-nova" then novaRun else
      abort "no run environment for ${system}";
in
{
  # Base
  #affinity = importRun ./repos/base/run/affinity.nix;
  #fpu      = importRun ./repos/base/run/fpu.nix;
  thread   = importRun ./repos/base/run/thread.nix;

  # OS
  #ldso   = importRun ./repos/os/run/ldso.nix;
  signal = importRun ./repos/os/run/signal.nix;
  timer  = importRun ./repos/os/run/timer.nix;
}