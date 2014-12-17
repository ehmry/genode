/*
 * \brief  Entry expression into components
 * \author Emery Hemingway
 * \date   2014-09-04
 */

{ system ? builtins.currentSystem
, spec ? import ./specs { inherit system; }
, tool ? import ./tool  { inherit spec; }
, libs ? import ./libs.nix { inherit system spec tool; }
}:

let

  # Link a component with ldso from libs.
  linkComponent = args:
  if tool.anyShared (args.libs or []) then
    tool.linkDynamicComponent (
      { dynamicLinker = libs.ld; } // args
    )
  else
    tool.linkStaticComponent args;

  callComponent = extraAttrs: f:
    f (
      builtins.intersectAttrs
        (builtins.functionArgs f)
        (tool // libs // { inherit spec linkComponent; } // extraAttrs)
    );

  importPkgs = p: import p { inherit tool callComponent; };

in
tool.mergeSets ([ { inherit libs; } ] ++ (
  map
    (repo: importPkgs (./repos + "/${repo}/pkgs.nix"))
    [ "base" "os" "demo" "libports" "ports" "gems" ]
))
