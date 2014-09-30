/*
 * \brief  Tools and utility functions
 * \author Emery Hemingway
 * \date   2014-09-30
 */

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

  fromDir = dir: names: map (name: "${dir}/${name}") names;

  newDir = name: contents:
    derivation {
      inherit name contents;
      system = builtins.currentSystem;
      builder = shell;
      PATH="${nixpkgs.coreutils}/bin";
      args = [ "-e" "-c" ''
        mkdir -p $out ; \
        for i in $contents; do cp -Hr $i $out; done
      '' ];
    };


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