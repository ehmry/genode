/*
 * \brief  Entry expression
 * \author Emery Hemingway
 * \date   2014-09-16
 */

{ system ? builtins.currentSystem }:

let

  nixpkgs = import <nixpkgs> {};

  tool = import ./tool { inherit system nixpkgs; };

  build = import ./tool/build { inherit system nixpkgs; };

  libs = import ./libs.nix {
    inherit system tool;
  };

in rec {

  pkgs = import ./pkgs.nix {
    inherit system tool libs;
  };

  run = import ./run.nix {
    inherit system tool pkgs;
  };

}