/*
 * \brief  Demo components
 * \author Emery Hemingway
 * \date   2014-09-15
 */

{ build, callPackage, baseIncludes, osIncludes, demoIncludes }:

let

  # overide the build.component function
  build' = build // {
    component = { includeDirs ? [], ... } @ args:
      build.component (args // {
        includeDirs =  builtins.concatLists [
          includeDirs demoIncludes osIncludes baseIncludes
        ];
      });
  };

  importComponent = path:
    callPackage (import path { build = build'; });

in
{
  server = {
    liquid_framebuffer = importComponent ./src/server/liquid_framebuffer;
    nitlog             = importComponent ./src/server/nitlog;
  };

  app = {
    launchpad = importComponent ./src/app/launchpad;
    scout     = importComponent ./src/app/scout;
  };

}