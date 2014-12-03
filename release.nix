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

  run = import ./run.nix {
    inherit system tool pkgs;
  };

in
{ test = builtins.removeAttrs run [ "demo" ]; }
//
( if system == "x86_32-nova" || system == "x86_64-nova"
  then { demo = import ./repos/demo/demo.nix { inherit tool pkgs; }; }
  else { }
)
