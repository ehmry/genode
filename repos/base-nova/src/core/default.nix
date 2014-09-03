{ base, repo }:

let
  sourceDir = repo.sourceDir + "/core";
in
{
  libs = [ base.lib.base-common ];

  sources =
    map (fn: sourceDir + "/${fn}")
      [ "core_rm_session.cc"
        "cpu_session_extension.cc"
        "cpu_session_support.cc"
        "echo.cc"
        "io_mem_session_support.cc"
        "irq_session_component.cc"
        "pd_session_extension.cc"
        "platform.cc"
        "platform_pd.cc"
        "platform_thread.cc"
        "ram_session_support.cc"
        "rm_session_support.cc"
        "signal_source_component.cc"
        "thread_start.cc"
      ]
    ++
    map (fn: base.sourceDir + "/core/${fn}")
      [ "main.cc"
        "context_area.cc"
        "core_mem_alloc.cc"
        "cpu_session_component.cc"
        "dataspace_component.cc"
        "dump_alloc.cc"
        "io_mem_session_component.cc"
        "pd_session_component.cc"
        "ram_session_component.cc"
        "rm_session_component.cc"
        "rom_session_component.cc"
        "signal_session_component.cc"
        "trace_session_component.cc"
        "version.cc"

        "/x86/io_port_session_component.cc"
        "/x86/platform_services.cc"
      ]
    ++
    [
      (base.sourceDir + "/base/console/core_printf.cc")
      (repo.sourceDir + "/base/pager/pager.cc")
    ];

  includeDirs =
    [ (sourceDir+"/include")
      (repo.sourceDir+"/platform")
      (repo.sourceDir+"/base/console")
    ];

  ldScriptStatic = repo.sourceDir + "/platform/roottask.ld";
  ldTextAddr = "0x100000";
}