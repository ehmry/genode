/*
 * \brief  Entry expression into libraries
 * \author Emery Hemingway
 * \date   2014-09-11
 */

{ build
, baseIncludes, osIncludes, demoIncludes }:

let
  callLibrary = f:
    f (builtins.intersectAttrs (builtins.functionArgs f) libs);

  base = import ./repos/base/libs.nix {
    inherit build callLibrary baseIncludes;
  };

  os = import ./repos/os/libs.nix {
    inherit build callLibrary baseIncludes osIncludes;
  };

  demo = import ./repos/demo/libs.nix {
    inherit build callLibrary baseIncludes osIncludes demoIncludes;
  };

  libs = base // os // demo;

in libs