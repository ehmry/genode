/*
 * \brief  Entry expression into components
 * \author Emery Hemingway
 * \date   2014-09-04
 */

{ build, libs
, baseIncludes, osIncludes, demoIncludes }:

let

  callPackage = f:
    f (builtins.intersectAttrs (builtins.functionArgs f) libs);

  base = import ./repos/base/pkgs.nix {
    inherit build callPackage baseIncludes;
  };

  os = import ./repos/os/pkgs.nix {
    inherit build callPackage baseIncludes osIncludes;
  };

  demo = import ./repos/demo/pkgs.nix {
    inherit build callPackage baseIncludes osIncludes demoIncludes;
  };

in base // os // demo // {
  server = (os.server // demo.server);
}