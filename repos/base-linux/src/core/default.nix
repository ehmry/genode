/*
* \author Emery Hemingway
* \date   2014-08-12
*/

{ build, base, repo }:

{
  libs = with base.lib;
    [ base-common cxx startup syscall ];

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
    ];

  ldScriptStatic =
    if builtins.hasAttr "alwaysHybrid" build.spec && build.spec.alwaysHybrid == true then
      [ (if build.is32Bit then "ldscripts/elf_i386.xc" else "ldscripts/elf_x86_64.xc")
        (repo.sourceDir + "/platform/context_area.stdlib.ld")
      ]
    else [ (base.sourceDir + "/platform/genode.ld") ];
}