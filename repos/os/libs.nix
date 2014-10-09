/*
 * \brief  OS libraries
 * \author Emery Hemingway
 * \date   2014-09-11
 */

{ tool, callLibrary }:

let

  # Prepare genodeEnv.
  genodeEnv =  tool.genodeEnvAdapters.addSystemIncludes
    tool.genodeEnv (
      ( import ../base/include { inherit (tool) genodeEnv; }) ++
      [ ./include ]);

  callLibrary' = callLibrary { inherit genodeEnv; };
  importLibrary = path: callLibrary' (import path);

in {
  alarm        = importLibrary ./src/lib/alarm;
  blit         = importLibrary ./src/lib/blit;
  config       = importLibrary ./src/lib/config;
  config_args  = importLibrary ./src/lib/config_args;
  init_pd_args = importLibrary ./src/lib/init_pd_args;
  ld           = importLibrary ./src/lib/ldso;
  ldso-arch    = importLibrary ./src/lib/ldso/arch;
  ldso-startup = importLibrary ./src/lib/ldso/startup;
  server       = importLibrary ./src/lib/server;
  timed_semaphore = importLibrary ./src/lib/timed_semaphore;
  timer           = importLibrary ./lib/mk/timer.nix;
  #trace        = importLibrary ./src/lib/trace;
}
