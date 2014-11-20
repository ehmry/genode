/*
 * \brief  Base packages
 * \author Emery Hemingway
 * \date   2014-09-04
 */

{ tool, callComponent }:

let

  # Prepare genodeEnv.
  genodeEnv = tool.genodeEnvAdapters.addSystemIncludes
    tool.genodeEnv (import ./include { inherit (tool) genodeEnv; });

  compileCC =
  attrs:
  tool.compileCC (attrs // {
    systemIncludes =
     (attrs.systemIncludes or [])
      ++
      (import ./include { inherit (tool) genodeEnv; });
  });

  callComponent' = callComponent { inherit genodeEnv compileCC; };

  importComponent = path: callComponent' (import path);
  
  callBasePackage = callComponent {
    inherit genodeEnv baseDir repoDir;
  };
  importBaseComponent = path: callBasePackage (import path);

  baseDir = ../base;
  repoDir = 
    if genodeEnv.isLinux then ../base-linux else
    if genodeEnv.isNova  then ../base-nova  else
    throw "no base components for ${genodeEnv.system}";

  impl = 
    import (repoDir+"/pkgs.nix") { inherit importBaseComponent; };

in
tool.mergeSet impl {
  test = {
    affinity = importComponent ./src/test/affinity;
    fpu      = importComponent ./src/test/fpu;
    thread   = importComponent ./src/test/thread;
  };
}
