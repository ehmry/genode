{ genodeEnv, baseDir, repoDir, cxx, startup }:

with genodeEnv.tool;

let
  subdir =
    if genodeEnv.isx86_32 then "x86_32" else
    if genodeEnv.isx86_64 then "x86_64" else
    abort "bad spec for ${genodeEnv.system}";
in
genodeEnv.mkLibrary {
  name = "base-common";
  libs = [ cxx startup ];

  # pager/x86_64/pager.cc

  sources =
    fromDir (repoDir+"/src/base")
      [ "env/cap_map.cc"
        "ipc/ipc.cc"
        "ipc/pager.cc"
        ("pager/"+subdir+"/pager.cc")
        "server/server.cc"
        "thread/thread_context.cc"
      ]
    ++
    fromDir (baseDir+"/src/base")
      [ "allocator/allocator_avl.cc"
        "allocator/slab.cc"
        "avl_tree/avl_tree.cc"
        "child/child.cc"
        "console/console.cc"
        "elf/elf_binary.cc"
        "heap/heap.cc"
        "heap/sliced_heap.cc"
        "lock/lock.cc"
        "process/process.cc"
        "signal/signal.cc"
        "signal/common.cc"
        "thread/context_allocator.cc"
        "thread/thread.cc"
        "thread/trace.cc"
      ];

  systemIncludes =
    [ (repoDir + "/src/base/lock")
      (baseDir + "/src/base/thread/")
    ];
}
