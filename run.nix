/*
 * \brief  Entry expression for runs
 * \author Emery Hemingway
 * \date   2014-09-16
 */

{ system, nixpkgs, pkgs, test }:

let
  importRun = path:
    let f = import path { inherit run; };
    in f (builtins.intersectAttrs (builtins.functionArgs f)
      ( pkgs // test ));

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
  # BASE
  affinity          = importRun ./repos/base/run/affinity.nix;
  fpu               = importRun ./repos/base/run/fpu.nix;
  thread            = importRun ./repos/base/run/thread.nix;
}