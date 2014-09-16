/*
 * \brief  OS libraries.
 * \author Emery Hemingway
 * \date   2014-09-11
 */

{ build, callLibrary, baseIncludes, osIncludes }:

let

  os = rec {
    sourceDir = ./src;
    includeDirs = osIncludes;
  };

  # overide the build.library function
  build' = build // {
    library = { includeDirs ? [], ... } @ args:
      build.library (args // {
        includeDirs =  builtins.concatLists [
          includeDirs osIncludes baseIncludes
        ];
      });
  };

  importLibrary = path:
    callLibrary (import path { build = build'; });

in {
  alarm        = importLibrary ./src/lib/alarm;
  blit         = importLibrary ./src/lib/blit;
  config       = importLibrary ./src/lib/config;
  config_args  = importLibrary ./src/lib/config_args;
  init_pd_args = importLibrary ./src/lib/init_pd_args;
  server       = importLibrary ./src/lib/server;
  #trace        = importLibrary ./src/lib/trace;
}