/*
 * \brief  Base libraries.
 * \author Emery Hemingway
 * \date   2014-09-11
 */

{ spec, tool, callLibrary }:

let
   baseIncludes = import ./include { inherit spec; inherit (tool) filterHeaders; };

  compileCC =
  attrs:
  tool.compileCC (attrs // {
    systemIncludes =
     (attrs.systemIncludes or []) ++ baseIncludes;
  });

  baseDir = ../base;
  repoDir =
    if spec.isFiasco then ../base-fiasco else
    if spec.isLinux  then ../base-linux  else
    if spec.isNova   then ../base-nova   else
    throw "no base libraries for ${spec.system}";

  callBaseLibrary = callLibrary {
    inherit compileCC baseDir repoDir baseIncludes;
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
      propagate.systemIncludes  = [ (tool.filterHeaders baseDir+"/src/base/env") ];
    };

  lock =
    { name = "lock";
      propagate.systemIncludes = [ (tool.filterHeaders baseDir+"/src/base/lock") ];
    };

  lx_hybrid.name = "lx_hybrid";

  syscall.name = "syscall";

} // impl # Overide with impl
