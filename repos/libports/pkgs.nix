/*
 * \brief  Libports components
 * \author Emery Hemingway
 * \date   2014-09-21
 */

{ tool, callPackage, baseIncludes, osIncludes, libportsIncludes }:

let

  tool' = tool // { inherit buildComponent; };

  # overide the build.component function
  buildComponent = { includeDirs ? [], ... } @ args:
    tool.build.component (args // {
      includeDirs = includeDirs ++
        libportsIncludes ++ osIncludes ++ baseIncludes;
    });

  importComponent = path:
    callPackage (import path { tool = tool'; });

in
{
  test =
    { ldso = importComponent ./src/test/ldso;
      libc = importComponent ./src/test/libc;
    };
}