/*
 * \brief  Base libraries.
 * \author Emery Hemingway
 * \date   2014-09-11
 */

{ spec, tool, callLibrary }:

let
  compileCC =
  attrs:
  tool.compileCC (attrs // {
    systemIncludes =
     (attrs.systemIncludes or [])
      ++
      (import ./include { inherit (tool) genodeEnv; });
  });

  baseDir = ../base;
  repoDir =
    if spec.isLinux then ../base-linux else
    if spec.isNova  then ../base-nova  else
    throw "no base libraries for ${tool.genodeEnv.system}";

  callBaseLibrary = callLibrary {
    inherit compileCC baseDir repoDir;
    baseIncludes = import ./include { inherit (tool) genodeEnv; };
  };
  importBaseLibrary = path: callBaseLibrary (import path);

  impl =
    import (repoDir+"/libs.nix") { inherit tool importBaseLibrary; };

in
{
  cxx          = importBaseLibrary ./src/base/cxx;
  ld           = importBaseLibrary ./src/lib/ldso;
  ldso-startup = importBaseLibrary ./src/lib/ldso/startup;
  startup      = importBaseLibrary ./lib/mk/startup.nix;

  ## Env is a fake library for propagating platform_env.h
  env =
    { name = "env";
      propagatedIncludes  = [ (tool.filterHeaders baseDir+"/src/base/env") ];
    };

  lock =
    { name = "lock";
      propagatedIncludes = [ (tool.filterHeaders baseDir+"/src/base/lock") ];
    };
  syscall = { name = "syscall"; };

} // impl # Overide with impl
