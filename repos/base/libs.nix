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

  baseDir = ../base;
  repoDir =
    if tool.genodeEnv.isLinux then ../base-linux else
    if tool.genodeEnv.isNova  then ../base-nova  else
    throw "no base libraries for ${tool.genodeEnv.system}";

  callBaseLibrary = callLibrary {
    inherit compileCC baseDir repoDir;
    baseIncludes = import ./include { inherit (tool) genodeEnv; };
  };
  importBaseLibrary = path: callBaseLibrary (import path);

  impl =
    import (repoDir+"/libs.nix") { inherit tool importBaseLibrary; };

in
impl // {
  cxx          = importBaseLibrary ./src/base/cxx;

  # Env is a fake library for propagating platform_env.h
  env =
    { name = "env";
      libs = [ ];
      shared = false;
      propagatedIncludes =
        [ ( tool.filterHeaders
             ( if tool.isLinux then repoDir+"/src/base/env"
               else baseDir+"/src/base/env"
             )
          )
        ];
    };

  ld           = importBaseLibrary ./src/lib/ldso;
  ldso-startup = importBaseLibrary ./src/lib/ldso/startup;
  startup      = importBaseLibrary ./lib/mk/startup.nix;
}
