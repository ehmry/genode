/*
 * \brief  Ports packages
 * \author Emery Hemingway
 * \date   2014-09-18
 */

{ tool, build, callPackage, baseIncludes, osIncludes }:

let

  # overide the build.component function
  build' = build // {
    component = { includeDirs ? [], ... } @ args:
      build.component (args // {
        includeDirs =  builtins.concatLists [
          includeDirs osIncludes baseIncludes
        ];
      });
  };

  importComponent = path:
    callPackage (import path { inherit tool; build = build'; });
    
  ports = import ./ports { inherit tool; };

in
{
  app = {
    dosbox = callPackage (import ./src/app/dosbox { inherit tool build; inherit (ports) dosbox; });
  };

}