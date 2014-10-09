/*
 * \brief  Demo libraries.
 * \author Emery Hemingway
 * \date   2014-09-15
 */

{ tool, callLibrary, includes }:

let

  # overide the build.library function
  buildLibrary =
  { includeDirs ? [], ... } @ args:
  tool.build.library (args // {
    includeDirs = includeDirs ++ includes;
  });

  tool' = tool // { inherit buildLibrary; };

  importLibrary = path:
    callLibrary (import path { tool = tool'; });

in {
  launchpad          = importLibrary ./src/lib/launchpad;
  libpng_static      = importLibrary ./src/lib/libpng;
  libz_static        = importLibrary ./src/lib/libz;
  mini_c             = importLibrary ./src/lib/mini_c;
  scout_gfx          = importLibrary ./src/lib/scout_gfx;
  scout_widgets      = importLibrary ./src/app/scout/lib.nix;
}