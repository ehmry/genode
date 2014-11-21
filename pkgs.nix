/*
 * \brief  Entry expression into components
 * \author Emery Hemingway
 * \date   2014-09-04
 */

{ system ? builtins.currentSystem
, tool ? import ./tool { inherit system; }
, libs ? import ./libs.nix { inherit system tool; }
}:

let
  callComponent = extraAttrs: f:
    f (
      builtins.intersectAttrs
        (builtins.functionArgs f)
        (libs // extraAttrs)
    );

  importPkgs = p: import p { inherit tool callComponent; };

in
tool.mergeSets ([ { inherit libs; } ] ++ (
  map
    (repo: importPkgs (./repos + "/${repo}/pkgs.nix"))
    [ "base" "os" "demo" "libports" ]
))
