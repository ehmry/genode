/*
 * \brief  Base tests
 * \author Emery Hemingway
 * \date   2014-09-10
 */

{ build, baseIncludes, callTest }:

let

 impl =
  if build.isLinux then { } else
  if build.isNova  then import ../base-nova/test.nix { inherit importTest; } else
  throw "no base tests for ${build.system}";

  # overide the build.component function
  build' = build // {
    component = { includeDirs ? [], ... } @ args:
      build.component (args // {
        includeDirs =  builtins.concatLists [
          includeDirs baseIncludes
        ];
      });
  };

  importTest = path:
    callTest (import path { build = build'; });

in {
  affinity = importTest ./src/test/affinity;
  fpu      = importTest ./src/test/fpu;
  thread   = importTest ./src/test/thread;
} // impl