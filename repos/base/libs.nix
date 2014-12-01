/*
 * \brief  Base libraries.
 * \author Emery Hemingway
 * \date   2014-09-11
 */

{ tool, callLibrary }:

let
  compileCC =
  attrs:
  tool.compileCC (attrs // {
    systemIncludes =
     (attrs.systemIncludes or [])
      ++
      (import ./include { inherit (tool) genodeEnv; });
  });

  callBaseLibrary = callLibrary {
    inherit (tool) genodeEnv compileS;
    inherit compileCC baseDir repoDir;
  };
  importBaseLibrary = path: callBaseLibrary (import path);

  baseDir = ../base;
  repoDir =
    if tool.genodeEnv.isLinux then ../base-linux else
    if tool.genodeEnv.isNova  then ../base-nova  else
    throw "no base libraries for ${tool.genodeEnv.system}";

  impl =
    import (repoDir+"/libs.nix") { inherit tool importBaseLibrary; };

in
impl // {
  cxx          = importBaseLibrary ./src/base/cxx;
  ld           = importBaseLibrary ./src/lib/ldso;
  ldso-startup = importBaseLibrary ./src/lib/ldso/startup;
  startup      = importBaseLibrary ./lib/mk/startup.nix;
}
