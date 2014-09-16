# Taken from base.mk and base.inc

{ repo }:
{ build, base, includeDirs }:

{ startup, cxx, base-common, syscall }:

build.library {
  name = "base";

  libs = [ startup cxx base-common syscall ];

  sources =
    map (fn: repo.sourceDir + "/base/${fn}")
      [ "thread/thread_linux.cc"
        "env/platform_env.cc"
      ]
    ++
    map (fn: base.sourceDir + "/base/${fn}")
      [ "console/log_console.cc"
        "env/context_area.cc"
        "env/env.cc"
        "thread/thread.cc"
      ];

  includeDirs =
    [ (repo.sourceDir + "/platform")
      (repo.sourceDir + "/base/env")
      (base.sourceDir + "/base/env")
    ] ++ includeDirs;

}