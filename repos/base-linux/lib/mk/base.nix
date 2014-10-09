#
# \brief  Base lib parts that are not used by hybrid applications
# \author Sebastian Sumpf
# \date   2014-02-21
#

{ genodeEnv, baseDir, repoDir, startup, cxx, base-common, syscall }:

genodeEnv.mkLibrary (genodeEnv.tool.mergeSets [
( import ./base.inc.nix {
   inherit genodeEnv baseDir repoDir base-common syscall cxx; 
})

{
  name = "base";
  libs = [ startup cxx ];

  sources = genodeEnv.fromPaths
    [ (baseDir+"/src/base/thread/thread.cc")
      (repoDir+"/src/base/thread/thread_linux.cc")
    ];

  systemIncludes =
    [ # thread_linux.cc - linux_syscalls.h
      (repoDir+"/src/platform") 
      (genodeEnv.toolchain.glibc+"/include")
    ];
}

])
