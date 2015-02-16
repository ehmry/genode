/*
 * \brief  Base libraries.
 * \author Emery Hemingway
 * \date   2014-09-11
 */

{ spec, tool, callLibrary
, baseIncludes, osIncludes, ... }:

let

  compileCC = tool.addIncludes [] (baseIncludes ++ osIncludes) tool.compileCC;

  baseDir = ../base;
  repoDir =
    if spec.isFiasco then ../base-fiasco else
    if spec.isLinux  then ../base-linux  else
    if spec.isNova   then ../base-nova   else
    throw "no base libraries for ${spec.system}";

  callBaseLibrary = callLibrary {
    inherit baseDir repoDir baseIncludes compileCC;
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
      propagate.includes  = [ (baseDir+"/src/base/env") ];
    };

  lock =
    { name = "lock";
      propagate.includes = [ (baseDir+"/src/base/lock") ];
    };

  lx_hybrid.name = "lx_hybrid";

  syscall.name = "syscall";

} // impl # Overide with impl
