/*
 * \brief  Entry expression
 * \author Emery Hemingway
 * \date   2014-09-16
 */

{ system ? builtins.currentSystem }:

let
  spec = import ./spec { inherit system; };
  tool = import ./tool  { inherit spec; };
  libs = import ./libs.nix { inherit system spec tool; };
in rec {

  pkgs = import ./pkgs.nix {
    inherit system spec tool libs;
  };

  run = import ./run.nix {
    inherit system spec tool pkgs;
  };

}
