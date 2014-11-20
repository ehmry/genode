{ genodeEnv, baseDir, repoDir }:

genodeEnv.mkLibrary {
  name = "syscall";
  sources =
    if genodeEnv.isArm then
      (genodeEnv.tool.fromDir (repoDir+"/src/platform/arm")
        [ "lx_clone.S" "lx_syscall.S" ])
    else
    if genodeEnv.isx86_32 then 
      (genodeEnv.tool.fromDir (repoDir+"/src/platform/x86_32")
        [ "lx_clone.S" "lx_syscall.S" ])
    else
    if genodeEnv.isx86_64 then 
      (genodeEnv.tool.fromDir (repoDir+"/src/platform/x86_64")
        [ "lx_clone.S" "lx_restore_rt.S" "lx_syscall.S" ])
    else
    throw "syscall library unavailable for ${genodeEnv.system}";

  propagatedIncludes =
    [ # linux_syscalls.h
      ../../src/platform
      (genodeEnv.tool.nixpkgs.linuxHeaders+"/include")
      (genodeEnv.toolchain.glibc+"/include")
    ];
}
