{ spec, tool, callLibrary
, baseIncludes, osIncludes, ... }:

let
  addIncludes = tool.addIncludes osIncludes baseIncludes;

  callLibrary' = callLibrary
    { inherit baseIncludes osIncludes;
      compileS  = addIncludes tool.compileS;
      compileC  = addIncludes tool.compileC;
      compileCC = addIncludes tool.compileCC;
    };

  importLibrary = path: callLibrary' (import path);

in {
  ahci         = importLibrary ./src/drivers/ahci/lib.nix;
  cli_monitor = importLibrary ./src/app/cli_monitor/lib.nix;
  net-stat     = importLibrary ./src/lib/net/net-stat.nix;
  timer           = importLibrary ./src/drivers/timer/lib.nix;
} // (tool.loadExpressions callLibrary' ./src/lib)
