# Taken from base.mk and base.inc

{ base, repo }:

{
  libs = with base.lib; [ startup cxx base-common syscall ];

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
    ];

}