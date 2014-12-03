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
        (libs // { inherit linkComponent; } // extraAttrs)
    );

  importPkgs = p: import p { inherit tool callComponent; };

  # Link a component.
  linkComponent = args:
  if tool.anyShared (args.libs or []) then
    tool.linkDynamicComponent (
       args // { dynamicLinker = args.dynamicLinker or libs.ld; }
     )
  else
    tool.linkStaticComponent args;
in
tool.mergeSets ([ { inherit libs; } ] ++ (
  map
    (repo: importPkgs (./repos + "/${repo}/pkgs.nix"))
    [ "base" "os" "demo" "libports" ]
))
