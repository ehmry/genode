{ genodeEnv, compileSubLibc }:
genodeEnv.mkLibrary {
  name = "libc-inet";
  externalObjects = compileSubLibc {
    sources = [ "lib/libc/inet/*.c" ];
  };
}
