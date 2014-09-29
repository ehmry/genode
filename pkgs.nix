/*
 * \brief  Entry expression into components
 * \author Emery Hemingway
 * \date   2014-09-04
 */

{ system ? builtins.currentSystem
, tool ? import ./tool { inherit system; }
, libs ? import ./libs.nix {
    inherit system tool;
    inherit baseIncludes osIncludes demoIncludes libportsIncludes;
  }
, baseIncludes     ? import ./repos/base/include     { inherit tool; }
, osIncludes       ? import ./repos/os/include       { inherit tool; }
, demoIncludes     ? import ./repos/demo/include     { inherit tool; }
, libportsIncludes ? import ./repos/libports/include { inherit tool; }
}:

let

  callPackage = f:
    f (builtins.intersectAttrs (builtins.functionArgs f) libs);

  base = import ./repos/base/pkgs.nix {
    inherit tool callPackage baseIncludes;
  };

  os = import ./repos/os/pkgs.nix {
    inherit tool callPackage baseIncludes osIncludes;
  };

  demo = import ./repos/demo/pkgs.nix {
    inherit tool callPackage baseIncludes osIncludes demoIncludes;
  };

  ports = import ./repos/ports/pkgs.nix {
    inherit tool callPackage baseIncludes osIncludes;
  };

# TODO: make a function to combine these
in base // os // demo // ports // {
  app = (demo.app // ports.app);
  server = (os.server // demo.server);
  test = (base.test // os.test);
}