{ genodeEnv, compileCC, baseDir, repoDir, syscall }:

let
  compileCC' = src: compileCC {
    inherit src;
    systemIncludes =
      [ # lock/lock.cc - spin_lock.h,lock_helper.h
        (repoDir+"/src/base/lock") (baseDir+"/src/base/lock")

        # platform_env.h - platform_env_common.h
        (repoDir+"/src/base/env") (baseDir+"/src/base/env")

        # thread/trace.cc - trace/control.h
        (baseDir+"/src/base/thread")

        # ipc.cc - <sys/un.h> <sys/socket.h>
        (genodeEnv.toolchain.glibc+"/include")

        syscall.include
      ];
  };
in
genodeEnv.mkLibrary {
  name = "base-common";
  libs = [ syscall ];
  objects = map compileCC' (
    (map (fn: (repoDir+"/src/base/${fn}"))
      [ "ipc/ipc.cc"
        "process/process.cc"
        "env/rm_session_mmap.cc"
        "env/debug.cc"
        "thread/thread_env.cc"
      ]
    ) ++
    (map (fn: (baseDir+"/src/base/${fn}"))
      [ "avl_tree/avl_tree.cc"
        "allocator/slab.cc"
        "allocator/allocator_avl.cc"
        "heap/heap.cc"
        "heap/sliced_heap.cc"
        "console/console.cc"
        "child/child.cc"
        "elf/elf_binary.cc"
        "lock/lock.cc"
        "signal/signal.cc" "signal/common.cc"
        "server/server.cc" "server/common.cc"
        "thread/trace.cc"
        "thread/context_allocator.cc"
      ]
    )
  );
}
