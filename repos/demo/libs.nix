/*
 * \brief  Demo libraries.
 * \author Emery Hemingway
 * \date   2014-09-15
 */

{ build, callLibrary, baseIncludes, osIncludes, demoIncludes }:

let

  # overide the build.library function
  build' = build // {
    library = { includeDirs ? [], ... } @ args:
      build.library (args // {
        includeDirs =  builtins.concatLists [
          includeDirs demoIncludes osIncludes baseIncludes
        ];
      });
  };

  importLibrary = path:
    callLibrary (import path { build = build'; });

in {
  launchpad          = importLibrary ./src/lib/launchpad;
  libpng_static      = importLibrary ./src/lib/libpng;
  libz_static        = importLibrary ./src/lib/libz;
  mini_c             = importLibrary ./src/lib/mini_c;
  scout_gfx          = importLibrary ./src/lib/scout_gfx;
  scout_widgets      = importLibrary ./src/app/scout/lib.nix;
}