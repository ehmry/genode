{ system ? builtins.currentSystem }:

let

  nixpkgs = import <nixpkgs> {};

  spec = import ./specs { inherit system; };
  tool = import ./tool  { inherit spec nixpkgs; };
  libs = import ./libs.nix { inherit system spec tool; };

  pkgs = import ./pkgs.nix {
    inherit system spec tool libs;
  };

  run = import ./run.nix {
    inherit system spec tool pkgs;
  };

in
{ test = builtins.removeAttrs run [ "demo" ];
  pkgs = builtins.removeAttrs pkgs [ "libs" ];
}
//
( if system == "x86_32-nova" || system == "x86_64-nova"
  then { demo = import ./repos/demo/demo.nix { inherit tool pkgs; }; }
  else { }
)
