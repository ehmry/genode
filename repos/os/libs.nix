/*
 * \brief  OS libraries
 * \author Emery Hemingway
 * \date   2014-09-11
 */

{ spec, tool, callLibrary }:

let
  importInclude = path: import path { inherit spec; };

  addIncludes =
  f: attrs:
  f (attrs // {
    includes =
     (attrs.includes or []) ++ (importInclude ../base/include);
  });

  importLibrary = path: callLibrary
    { osInclude = importInclude ./include;
      compileS  = addIncludes tool.compileS;
      compileC  = addIncludes tool.compileC;
      compileCC = addIncludes tool.compileCC;
    } (import path);

in {
  ahci         = importLibrary ./src/drivers/ahci/lib.nix;
  alarm        = importLibrary ./src/lib/alarm;
  blit         = importLibrary ./src/lib/blit;
  config       = importLibrary ./src/lib/config;
  config_args  = importLibrary ./src/lib/config_args;
  dde_kit      = importLibrary ./src/lib/dde_kit;
  init_pd_args = importLibrary ./src/lib/init_pd_args;
  net-stat     = importLibrary ./src/lib/net/net-stat.nix;
  server       = importLibrary ./src/lib/server;
  timed_semaphore = importLibrary ./src/lib/timed_semaphore;
  timer           = importLibrary ./src/drivers/timer/lib.nix;
  #trace        = importLibrary ./src/lib/trace;
}
