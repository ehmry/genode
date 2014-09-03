/*
 * \brief  Expression for the demo repo
 * \author Emery Hemingway
 * \date   2014-08-14
 */

{ tool, base, os }:

let
  inherit (tool) build;

  importComponent = p: import p { inherit build base os demo; };

  demo = rec {
    includeDir = ./include;
    includeDirs = [ includeDir ];

   lib =
     { launchpad     = importComponent ./src/lib/launchpad;
       scout_gfx     = importComponent ./src/lib/scout_gfx;
       scout_widgets = importComponent ./src/app/scout/lib.nix;

       libpng_static = importComponent ./src/lib/libpng;
       libz_static   = importComponent ./src/lib/libz;
       mini_c        = importComponent ./src/lib/mini_c;
     };

   server =
     { liquid_framebuffer = importComponent ./src/server/liquid_framebuffer;
       nitlog = importComponent ./src/server/nitlog;
     };

   app =
     { launchpad = importComponent ./src/app/launchpad;
       scout     =  importComponent ./src/app/scout;
     };
  };

in
demo // {

  merge = 
  { base, os, ... }:
  {
    inherit (demo) lib server app;

    test =
      { libpng_static = importComponent ./src/lib/libpng/test.nix;
      };
  };

}