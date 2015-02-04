#
# \brief  Portions of base library that are exclusive to non-core processes
# \author Norman Feske
# \date   2013-02-14
#
# The content of this file is used for both native Genode as well as hybrid
# Linux/Genode programs. Hence, it must be void of any thread-related code.
#


{ genodeEnv, compileCC, baseDir, repoDir, baseIncludes, base-common, syscall, env }:

let
  includes =
    [ # There is a potential problem here, both of the
      # these directories have files named platform_env.h
      #
      # platform_env.h - platform_env_common.h
      (repoDir+"/src/base/env")
      (baseDir+"/src/base/env")
    ] ++ baseIncludes;
in
{
  libs = [ base-common syscall ];

  objects = map (src: compileCC { inherit src includes; libs = [ env syscall ]; })
    [ (repoDir+"/src/base/env/platform_env.cc")

      (baseDir+"/src/base/console/log_console.cc")
      (baseDir+"/src/base/env/env.cc")
      (baseDir+"/src/base/env/context_area.cc")
    ];
}
