{ genodeEnv, baseDir, repoDir, syscall }:

let sourceDir = baseDir+"/src/platform"; in
genodeEnv.mkLibrary {
  name = "startup";
  shared = false;

  libs = if syscall == null then [] else [ syscall ];

  sources =
    genodeEnv.fromDir sourceDir [ "_main.cc" "init_main_thread.cc" ]
    ++
    genodeEnv.fromPaths
      [ (( if genodeEnv.isArm    then sourceDir+"/arm" else
           if genodeEnv.isx86_32 then sourceDir+"/x86_32" else
           if genodeEnv.isx86_64 then sourceDir+"/x86_64" else
           abort "no startup library for ${genodeEnv.system}"
         ) + "/crt0.s")
      ];
}
