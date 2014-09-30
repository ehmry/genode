/*
 * \brief  OS libraries
 * \author Emery Hemingway
 * \date   2014-09-11
 */

{ tool, callLibrary, baseIncludes, osIncludes }:

let

  os = rec {
    sourceDir = ./src;
    includeDirs = osIncludes;
  };

  tool' = tool // { inherit buildLibrary; };

  # overide the build.library function
  buildLibrary = { includeDirs ? [], ... } @ args:
    tool.build.library (args // {
      includeDirs =  builtins.concatLists [
        includeDirs osIncludes baseIncludes
      ];
    });

  importLibrary = path:
    callLibrary (import path { tool = tool'; });

in {
  alarm        = importLibrary ./src/lib/alarm;
  blit         = importLibrary ./src/lib/blit;
  config       = importLibrary ./src/lib/config;
  config_args  = importLibrary ./src/lib/config_args;
  init_pd_args = importLibrary ./src/lib/init_pd_args;
  ld           = importLibrary ./src/lib/ldso;
  ldso-startup = importLibrary ./src/lib/ldso/startup;
  ldso-arch    = importLibrary ./src/lib/ldso/arch;
  server       = importLibrary ./src/lib/server;
  timed_semaphore = importLibrary ./src/lib/timed_semaphore;
  #trace        = importLibrary ./src/lib/trace;
}