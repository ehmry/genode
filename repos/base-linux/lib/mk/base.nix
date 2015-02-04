#
# \brief  Base lib parts that are not used by hybrid applications
# \author Sebastian Sumpf
# \date   2014-02-21
#

{ genodeEnv, linkStaticLibrary, compileCC
, baseDir, repoDir, baseIncludes
, startup, cxx, base-common, syscall, env }:

linkStaticLibrary (genodeEnv.tool.mergeSets [
  ( import ./base.inc.nix {
     inherit genodeEnv compileCC baseDir repoDir baseIncludes base-common syscall env;
  })

  rec {
    name = "base";
    libs = [ startup cxx ];

    objects = map 
      (src: compileCC { inherit src libs; includes = baseIncludes; })
      [ (baseDir+"/src/base/thread/thread.cc")
        (repoDir+"/src/base/thread/thread_linux.cc")
      ];
    propagate.includes = baseIncludes;
  }
])
