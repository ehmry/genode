/*
 * \brief  Demo libraries.
 * \author Emery Hemingway
 * \date   2014-09-15
 */

{ tool, callLibrary }:

let

  compileCC =
  attrs:
  tool.compileCC (attrs // {
    systemIncludes =
     (attrs.systemIncludes or []) ++
      (import ../base/include { inherit (tool) genodeEnv; }) ++
      (import ./include { inherit (tool) genodeEnv; });
  });

  callLibrary' = callLibrary {
    inherit (tool) genodeEnv compileC transformBinary;
    inherit compileCC;
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
