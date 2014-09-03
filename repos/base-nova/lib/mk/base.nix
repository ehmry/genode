{ base, repo }:

{
  libs = with base.lib; [ base-common ];

  sources =
    map (fn: repo.sourceDir + "/base/${fn}")
      [ "thread/thread_nova.cc" ]
    ++
    map (fn: base.sourceDir + "/base/${fn}")
      [ "console/log_console.cc"
        "cpu/cache.cc"
        "env/context_area.cc"
        "env/env.cc"
        "env/reinitialize.cc"
      ];

  includeDirs = [ (base.sourceDir + "/base/env") ];

}