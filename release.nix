{ system ? builtins.currentSystem }:

let

  nixpkgs = import <nixpkgs> {};

  tool = import ./tool { inherit system nixpkgs; };

  build = import ./tool/build { inherit system nixpkgs; };

  libs = import ./libs.nix {
    inherit system tool;
  };

  pkgs = import ./pkgs.nix {
    inherit system tool libs;
  };
in
{
  test = import ./run.nix {
    inherit system tool pkgs;
  };
}
