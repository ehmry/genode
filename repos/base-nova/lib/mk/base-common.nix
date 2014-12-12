{ genodeEnv, linkStaticLibrary, compileCC, baseDir, repoDir, cxx, startup }:

let
  compileCC' = src: compileCC {
    inherit src;
    systemIncludes =
      [ (repoDir + "/src/base/lock")
        #(baseDir + "/src/base/lock")
        (baseDir + "/src/base/thread")
      ];
  };

  subdir =
    if genodeEnv.isx86_32 then "x86_32" else
    if genodeEnv.isx86_64 then "x86_64" else
    abort "bad spec for ${genodeEnv.system}";
in
linkStaticLibrary {
  name = "base-common";
  libs = [ cxx startup ];

  objects = map compileCC' (
    (map (fn: (repoDir+"/src/base/${fn}"))
      [ "env/cap_map.cc"
        "ipc/ipc.cc"
        "ipc/pager.cc"
        ("pager/"+subdir+"/pager.cc")
        "server/server.cc"
        "thread/thread_context.cc"
      ]
    ) ++
    ( map (fn: (baseDir+"/src/base/${fn}"))
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
      ]
    )
  );
}
