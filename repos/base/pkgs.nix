/*
 * \brief  Base packages.
 * \author Emery Hemingway
 * \date   2014-09-04
 */

{ build, baseIncludes, callPackage }:

let

  impl =
    if build.isLinux then import ../base-linux/pkgs.nix { inherit build base; } else
    if build.isNova  then import ../base-nova/pkgs.nix  { inherit build base; } else
    throw "no base support for ${build.system}";

  base = {
    sourceDir = ./src;
    includeDirs = baseIncludes;
  };

  # overide the build.component function
  build' = build // {
    component = { includeDirs ? [], ... } @ args:
      build.component (args // {
        includeDirs =  builtins.concatLists [
          includeDirs baseIncludes
        ];
      });
  };

  importComponent = path:
    callPackage (import path { build = build'; });

  ## TODO: append the git revision
  version = builtins.readFile ../../VERSION;

in
{
  test = {
    affinity = importComponent ./src/test/affinity;
    fpu      = importComponent ./src/test/fpu;
    thread   = importComponent ./src/test/thread;
  } // (impl.test or {});

  core = callPackage (impl.core {
    name = "core";
    includeDirs =
      [ (base.sourceDir + "/core/include") ]
      ++ map (d: base.sourceDir + "/base/${d}") [ "env" "thread" ]
      ++ base.includeDirs;
    ccOpt = build.ccOpt ++ (impl.core.flags or []) ++ [ "-DGENODE_VERSION='\"${version}\"'" ];
  });

}