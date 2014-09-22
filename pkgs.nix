/*
 * \brief  Entry expression into components
 * \author Emery Hemingway
 * \date   2014-09-04
 */

{ tool, build, libs
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

  ports = import ./repos/ports/pkgs.nix {
    inherit tool build callPackage baseIncludes osIncludes;
  };

# TODO: make a function to combine these
in base // os // demo // ports // {
  app = (demo.app // ports.app);
  server = (os.server // demo.server);
  test = (base.test // os.test);
}