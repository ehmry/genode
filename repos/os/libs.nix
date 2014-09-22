/*
 * \brief  OS libraries.
 * \author Emery Hemingway
 * \date   2014-09-11
 */

{ tool, callLibrary, baseIncludes, osIncludes }:

let

  os = rec {
    sourceDir = ./src;
    includeDirs = osIncludes;
  };

  # overide the build.library function
  build = tool.build // {
    library = { includeDirs ? [], ... } @ args:
      tool.build.library (args // {
        includeDirs =  builtins.concatLists [
          includeDirs osIncludes baseIncludes
        ];
      });
  };

  importLibrary = path:
    callLibrary (import path { inherit build; });

in {
  alarm        = importLibrary ./src/lib/alarm;
  blit         = importLibrary ./src/lib/blit;
  config       = importLibrary ./src/lib/config;
  config_args  = importLibrary ./src/lib/config_args;
  init_pd_args = importLibrary ./src/lib/init_pd_args;
  ldso-startup = importLibrary ./src/lib/ldso/startup;
  server       = importLibrary ./src/lib/server;
  #trace        = importLibrary ./src/lib/trace;
}