/*
 * \brief  Demo libraries.
 * \author Emery Hemingway
 * \date   2014-09-15
 */

{ spec, tool, callLibrary
, baseIncludes, osIncludes, ... }:

let
  addIncludes =
  f: attrs:
  f (attrs // {
    includes =
     (attrs.includes or []) ++ [ ./include ];
    externalIncludes =
     (attrs.externalIncludes or []) ++ osIncludes ++ baseIncludes;
  });

  callLibrary' = callLibrary {
    compileCC = addIncludes tool.compileCC;
  };

  importLibrary = path: callLibrary' (import path);

in {
  launchpad          = importLibrary ./src/lib/launchpad;
  libpng_static      = importLibrary ./src/lib/libpng;
  libz_static        = importLibrary ./src/lib/libz;
  mini_c             = importLibrary ./src/lib/mini_c;
  scout_gfx          = importLibrary ./src/lib/scout_gfx;
  scout_widgets      = importLibrary ./src/app/scout/lib.nix;
}
