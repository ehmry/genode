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

  base     = importPkgs ./repos/base/pkgs.nix;
  os       = importPkgs ./repos/os/pkgs.nix;
  libports = importPkgs ./repos/libports/pkgs.nix;

  #demo = import ./repos/demo/pkgs.nix {
  #  inherit tool callComponent baseIncludes osIncludes demoIncludes;
  #};



  #ports = import ./repos/ports/pkgs.nix {
  #  inherit tool callComponent baseIncludes osIncludes;
  #};

in tool.mergeSets [ base os libports { inherit libs system; } ]
