{ genodeEnv, compileCC
, baseDir, repoDir, versionObject
, base-common, cxx, startup, syscall }:

let
  genCoreDir = baseDir+"/src/core";

  systemIncludes =
    [ (repoDir + "/src/core/include") (genCoreDir + "/include")
      (repoDir + "/src/base/env") (baseDir + "/src/base/env")
      (baseDir + "/src/base/thread")
      (repoDir + "/src/base/console")
      (repoDir + "/src/base/ipc")

      (repoDir + "/src/platform")

      # main.cc - core_env.h - platform_env.h - unistd.h ..
      (genodeEnv.toolchain.glibc+"/include")
    ];

in
genodeEnv.mkComponent {
  name = "core";
  libs = [ base-common cxx startup syscall ];

  objects = map (src: compileCC { inherit src systemIncludes; }) (
    map (fn: genCoreDir+"/${fn}")
      [ "main.cc"
        "cpu_session_component.cc"
        "ram_session_component.cc"
        "platform_services.cc"
        "signal_session_component.cc"
        "signal_source_component.cc"
        "trace_session_component.cc"
      ]
    ++
    [ (baseDir+"/src/base/console/core_printf.cc")
      (baseDir+"/src/base/thread/thread.cc")
    ]
    ++
    map (fn: repoDir+"/src/core/${fn}")
      [ "platform.cc"
        "platform_thread.cc"
        "ram_session_support.cc"
        "rom_session_component.cc"
        "cpu_session_extension.cc"
        "cpu_session_support.cc"
        "pd_session_component.cc"
        "io_mem_session_component.cc"
        "thread_linux.cc"
        "context_area.cc"
      ]
  ) ++ [ versionObject ];

  # HOST_INC_DIR += /usr/include

  #
  # core does not use POSIX threads when built for the
  # 'lx_hybrid_x86' platform, so we need to reserve the
  # thread-context area via a segment in the program to
  # prevent clashes with vdso and shared libraries.
  #
  ldScriptsStatic = if genodeEnv.spec.hydrid or false
    then
      genodeEnv.spec.ldScriptsStatic ++
      [ (repoDir+"src/platform/context_area.stdlib.ld") ]
    else
      genodeEnv.spec.ldScriptsStatic;
}
