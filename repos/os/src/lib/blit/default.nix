{ genodeEnv, compileCC }:

genodeEnv.mkLibrary {
  name = "blit";
  objects = compileCC {
    src = ./blit.cc;
    systemIncludes =
      if genodeEnv.isArm then [ ./arm ] else
      if genodeEnv.isx86_32 then [ ./x86 ./x86/x86_32 ] else
      if genodeEnv.isx86_64 then [ ./x86 ./x86/x86_64 ] else
      throw "no blit library for build.system" ++
      [ ../blit ];
  };
}
