/*
 * \brief  Entry expression
 * \author Emery Hemingway
 * \date   2014-09-16
 */

{ system ? builtins.currentSystem }:

let

  nixpkgs = import <nixpkgs> {};

  tool = import ./tool { inherit nixpkgs; };

  build = import ./tool/build { inherit system nixpkgs; };

  baseIncludes = import ./repos/base/include { inherit tool; };
  osIncludes   = import ./repos/os/include   { inherit tool; };
  demoIncludes = import ./repos/demo/include { inherit tool; };

  libs = import ./libs.nix {
    inherit system tool baseIncludes osIncludes demoIncludes;
  };

in rec {

  pkgs = import ./pkgs.nix {
    inherit tool build libs baseIncludes osIncludes demoIncludes;
  };

  run = import ./run.nix {
    inherit system nixpkgs pkgs;
  };

}