/*
 * \brief  NOVA core
 * \author Emery Hemingway
 * \date   2014-10-21
 */

{ genodeEnv, baseDir, repoDir, base-common }:

let
  genCoreDir = baseDir+"/src/core";
in
genodeEnv.mkComponent  (genodeEnv.tool.mergeSets [ {
  name = "core";
  libs = [ base-common ];

  ldScriptStatic = repoDir + "/src/platform/roottask.ld";
  ldTextAddr = "0x100000";

  sources =
    genodeEnv.fromDir genCoreDir
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
    genodeEnv.fromDir (genCoreDir+"/x86")
      [ "io_port_session_component.cc"
        "platform_services.cc"
      ]
    ++
    genodeEnv.fromPath (baseDir+"/src/base/console/core_printf.cc")
    ++
    genodeEnv.fromPath (repoDir+"/src/base/pager/pager.cc")
    ++
    genodeEnv.fromDir (repoDir+"/src/core")
      [ "core_rm_session.cc"
        "cpu_session_extension.cc"
        "cpu_session_support.cc"
        "echo.cc"
        "irq_session_component.cc"
        "io_mem_session_support.cc"        
        "pd_session_extension.cc"
        "platform.cc"
        "platform_pd.cc"
        "platform_thread.cc"
        "ram_session_support.cc"
        "rm_session_support.cc"
        "signal_source_component.cc"
        "thread_start.cc"
      ];

  systemIncludes =
    [ (repoDir + "/src/core/include")
      #(repoDir + "/src/base/env") (baseDir + "/src/base/env")
      (baseDir + "/src/base/thread")
      (repoDir + "/src/base/console")
      #(repoDir + "/src/base/ipc")
      (genCoreDir + "/include")
    ];

} (import (genCoreDir+"/version.nix")) ])
