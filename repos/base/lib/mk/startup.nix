{ genodeEnv, compileCC, compiles, baseDir, repoDir, syscall }:

let
  sourceDir = baseDir+"/src/platform";
  archDir =
    if genodeEnv.isArm    then sourceDir+"/arm" else
    if genodeEnv.isx86_32 then sourceDir+"/x86_32" else
    if genodeEnv.isx86_64 then sourceDir+"/x86_64" else
    abort "no startup library for ${genodeEnv.system}";

  systemIncludes = map (d: d+"/src/platform") [ repoDir baseDir ];

in
genodeEnv.mkLibrary {
  name = "startup";

  libs = [ syscall ];

  objects =
    (map (src: compileCC { inherit src systemIncludes; })
      [ (sourceDir+"/_main.cc") (sourceDir+"/init_main_thread.cc") ]
    ) ++
    [ (compiles { src = (archDir+"/crt0.s"); }) ];
}
