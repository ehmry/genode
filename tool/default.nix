{ nixpkgs ? import <nixpkgs> { } }:

let
  shell = nixpkgs.bash + "/bin/sh";
in
{
  inherit nixpkgs;

  filterOut = filters: filteree:
    builtins.filter (e: !(builtins.elem e filters)) filteree;

  wildcard = glob:
    import (derivation {
      name = "files.nix";
      system = builtins.currentSystem;
      builder = shell;
      inherit glob;
      args = [ "-c" "echo [ $glob ] > $out" ];
    });

  preparePort = import ./prepare-port { inherit nixpkgs; };

}