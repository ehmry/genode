{ genodeEnv, compileSubLibc }:

genodeEnv.mkLibrary {
  name = "libc-stdio";
  externalObjects = compileSubLibc {
     sources = [ "lib/libc/stdio/*.c" ];
  };
}
