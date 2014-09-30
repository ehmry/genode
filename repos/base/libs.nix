/*
 * \brief  Base libraries.
 * \author Emery Hemingway
 * \date   2014-09-11
 */

{ tool, callLibrary, baseIncludes }:

let

  impl = 
    if build.isLinux then import ../base-linux/libs.nix { inherit build callLibrary base; } else
    if build.isNova  then import ../base-nova/libs.nix  { inherit build callLibrary base; } else
    throw "no base support for ${build.system}";

  base = {
    sourceDir = ./src;
    includeDirs = baseIncludes;
  };

  # overide the build.library function
  build = tool.build // {
    library = { includeDirs ? [], ... } @ args:
      tool.build.library (args // {
        includeDirs =  builtins.concatLists [
          includeDirs baseIncludes
        ];
      });
  };

  importLibrary = path:
    callLibrary (import path { inherit build; });

  mergeLib = { name, includeDirs ? [] }:
    let x = builtins.getAttr name impl;
    in callLibrary (x {inherit build base includeDirs; });

in (impl // {
  repo = null; # hack

    base = mergeLib {
      name = "base";
      inherit (base) includeDirs;
    };

    base-common = mergeLib {
      name = "base-common";
      includeDirs = (
        map (d: base.sourceDir + "/base/${d}") [ "env" "lock" "thread" ]
        ++ base.includeDirs);
    };

    cxx = importLibrary ./src/base/cxx;
    startup = callLibrary (import ./lib/mk/startup.nix { inherit build base impl;});
})