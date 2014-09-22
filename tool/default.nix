{ system ? builtins.currentSystem
,  nixpkgs ? import <nixpkgs> { }
}:

let
  shell = nixpkgs.bash + "/bin/sh";
in
{
  inherit nixpkgs;

  filterOut = filters: filteree:
    builtins.filter (e: !(builtins.elem (builtins.baseNameOf e) filters)) filteree;

  wildcard = glob:
    import (derivation {
      name = "files.nix";
      system = builtins.currentSystem;
      builder = shell;
      PATH="${nixpkgs.coreutils}/bin";
      inherit glob;
      args = [ "-e" "-c" "echo [ > $out; ls -Q $glob >> $out; echo ] >> $out" ];
    });

  preparePort = import ./prepare-port { inherit nixpkgs; };

  build = import ./build { inherit system nixpkgs; };

}