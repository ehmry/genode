/*
 * \brief  Entry expression into tests
 * \author Emery Hemingway
 * \date   2014-09-10
 */

{ build, libs
, baseIncludes, osIncludes, demoIncludes }:

let

  callTest = f:
    f (builtins.intersectAttrs (builtins.functionArgs f) libs);

  base = import ./repos/base/tests.nix {
    inherit build callTest baseIncludes;
  };

  os = import ./repos/os/tests.nix {
    inherit build callTest baseIncludes osIncludes;
  };

  demo = import ./repos/demo/tests.nix {
    inherit build callTest baseIncludes osIncludes demoIncludes;
  };

in base