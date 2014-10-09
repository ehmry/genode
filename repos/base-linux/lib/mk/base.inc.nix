#
# \brief  Portions of base library that are exclusive to non-core processes
# \author Norman Feske
# \date   2013-02-14
#
# The content of this file is used for both native Genode as well as hybrid
# Linux/Genode programs. Hence, it must be void of any thread-related code.
#

{ genodeEnv, baseDir, repoDir, base-common, syscall, cxx }:

{
  libs = [ base-common syscall cxx ];

  sources =
    genodeEnv.fromDir (repoDir+"/src/base")
      [ "env/platform_env.cc" ]
    ++
    genodeEnv.fromDir (baseDir+"/src/base")
      [ "console/log_console.cc" "env/env.cc" "env/context_area.cc" ];

  systemIncludes =
    [ # There is a potential problem here, both of the
      # these directories have files named platform_env.h
      #
      # platform_env.h - platform_env_common.h
      (repoDir+"/src/base/env")
      (baseDir+"/src/base/env")
    ];
}
