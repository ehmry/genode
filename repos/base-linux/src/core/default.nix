/*
* \brief  Functions for Linux core expression.
* \author Emery Hemingway
* \date   2014-08-12
*
*/

{ build, base, repo }:

{ name, ccOpt, includeDirs }:

{ base-common, cxx, startup, syscall }:

build.component {
  inherit name ccOpt;

  libs = [ base-common cxx startup syscall ];

  sources =
    map (fn: repo.sourceDir + "/core/${fn}")
    [ "context_area.cc"
      "cpu_session_extension.cc"
      "cpu_session_support.cc"
      "io_mem_session_component.cc"
      "pd_session_component.cc"
      "platform.cc"
      "platform_thread.cc"
      "ram_session_support.cc"
      "rom_session_component.cc"
      "thread_linux.cc"
    ]
    ++
    map (fn: base.sourceDir + "/core/${fn}")
      [ "main.cc"
        "cpu_session_component.cc"
        "platform_services.cc"
        "ram_session_component.cc"
        "signal_session_component.cc"
        "signal_source_component.cc"
        "trace_session_component.cc"
        "version.cc"
      ]
    ++
    [ (base.sourceDir + "/base/console/core_printf.cc")
      (base.sourceDir + "/base/thread/thread.cc")
      #(base.sourceDir + "/base/thread/trace.cc")
    ];

  includeDirs =
    [ (repo.sourceDir + "/core/include")
      (base.sourceDir + "/core/include")
      (repo.sourceDir + "/platform")
      (repo.sourceDir + "/base/ipc")
      (repo.sourceDir + "/base/env")
      (repo.sourceDir + "/base/console")
      (base.sourceDir + "/base/thread")
      #"${build.nixpkgs.linuxHeaders}/include"
    ] ++ includeDirs;

}