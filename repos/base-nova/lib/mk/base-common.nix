{ build, base, repo }:

let
  subdir =
    if build.isx86_32 then "x86_32" else
    if build.isx86_64 then "x86_64" else
    abort "bad spec for ${build.spec.system}";
in
{
  libs = with base.lib; [ cxx startup ];

  sources =
    map (fn: repo.sourceDir + "/base/${fn}")
      [ "env/cap_map.cc"
        "ipc/ipc.cc"
        "ipc/pager.cc"
        ("pager/"+subdir+"/pager.cc")
        "server/server.cc"
        "thread/thread_context.cc"
      ]
    ++
    map (fn: base.sourceDir + "/base/${fn}")
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

  includeDirs =
    [ (base.sourceDir + "/base/elf/")
      (repo.sourceDir + "/base/lock")
    ];
}