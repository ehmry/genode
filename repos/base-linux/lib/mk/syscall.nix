{ genodeEnv, linkStaticLibrary, compileS, baseDir, repoDir, filterHeaders, nixpkgs }:

linkStaticLibrary {
  name = "syscall";
  objects =
    if genodeEnv.isArm then map
        (fn: compileS { src = repoDir+"/src/platform/arm/${fn}"; })
        [ "lx_clone.S" "lx_syscall.S" ]
    else
    if genodeEnv.isx86_32 then map
        (fn: compileS { src = repoDir+"/src/platform/x86_32/${fn}"; })
        [ "lx_clone.S" "lx_syscall.S" ]
    else
    if genodeEnv.isx86_64 then map
        (fn: compileS { src = repoDir+"/src/platform/x86_64/${fn}"; })
        [ "lx_clone.S" "lx_restore_rt.S" "lx_syscall.S" ]
    else
    throw "syscall library unavailable for ${genodeEnv.system}";

    propagate.systemIncludes =
        [ (filterHeaders (repoDir + /src/platform))
          "${genodeEnv.toolchain.libc}/include"
          "${nixpkgs.linuxHeaders}/include"
        ];
}
