{ base, repo }:

{
  libs = [ base.lib.syscall ];

  sources =
    map (fn: repo.sourceDir + "/base/${fn}")
      [ "env/rm_session_mmap.cc" "env/debug.cc"
        "ipc/ipc.cc"
        "process/process.cc"
        "thread/thread_env.cc"
      ]
    ++
    map (fn: base.sourceDir + "/base/${fn}")
      [ "allocator/allocator_avl.cc" "allocator/slab.cc"
        "avl_tree/avl_tree.cc"
        "child/child.cc"
        "console/console.cc"
        "elf/elf_binary.cc"
        "heap/heap.cc" "heap/sliced_heap.cc"
        "lock/lock.cc"
        "signal/signal.cc"
        "server/common.cc" "server/server.cc"
        "signal/common.cc"
        "thread/context_allocator.cc" "thread/trace.cc"
      ];

  includeDirs =
    [ (base.sourceDir + "/base/elf")
      (repo.sourceDir + "/platform" )
    ]
    ++
    map (d: repo.sourceDir + "/base/${d}") [ "ipc" "env" "lock" ];

}