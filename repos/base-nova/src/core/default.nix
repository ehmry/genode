{ genodeEnv, compileCC
, baseDir, repoDir, versionObject
, base-common }:

let
  genCoreDir = baseDir+"/src/core";

  systemIncludes =
    [ (repoDir + "/src/core/include")
      (repoDir + "/src/base/console")
      (baseDir + "/src/base/thread")
      (genCoreDir + "/include")
    ];
in
genodeEnv.mkComponent {
  name = "core";
  libs = [ base-common ];

  ldScriptsStatic = [ (repoDir + "/src/platform/roottask.ld") ];
  ldTextAddr = "0x100000";

  objects = map (src: compileCC { inherit src systemIncludes; }) (
    map (fn: genCoreDir+"/${fn}")
      [ "main.cc"
        "context_area.cc"
        "core_mem_alloc.cc"
        "cpu_session_component.cc"
        "dataspace_component.cc"
        "dump_alloc.cc"
        "io_mem_session_component.cc"
        "ram_session_component.cc"
        "rom_session_component.cc"
        "pd_session_component.cc"
        "rm_session_component.cc"
        "signal_session_component.cc"
        "trace_session_component.cc"
      ]
    ++
    map (fn: genCoreDir+"/x86/${fn}")
      [ "io_port_session_component.cc"
        "platform_services.cc"
      ]
    ++
    [ (baseDir+"/src/base/console/core_printf.cc")
      (repoDir+"/src/base/pager/pager.cc")
    ]
    ++
    map (fn: repoDir+"/src/core/${fn}")
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
  ) ++ [ versionObject ];

}
