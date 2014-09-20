/*
 * \brief  Entry expression into libraries
 * \author Emery Hemingway
 * \date   2014-09-11
 */

{ system ? builtins.currentSystem
, tool ? import ./tool { inherit system; }
, baseIncludes ? import ./repos/base/include { inherit tool; }
, osIncludes   ? import ./repos/os/include   { inherit tool; }
, demoIncludes ? import ./repos/demo/include { inherit tool; }
}:

let
  callLibrary = f:
    f (builtins.intersectAttrs (builtins.functionArgs f) libs);

  base = import ./repos/base/libs.nix {
    inherit tool callLibrary baseIncludes;
  };

  os = import ./repos/os/libs.nix {
    inherit tool callLibrary baseIncludes osIncludes;
  };

  demo = import ./repos/demo/libs.nix {
    inherit tool callLibrary baseIncludes osIncludes demoIncludes;
  };

  libs = base // os // demo;

in libs