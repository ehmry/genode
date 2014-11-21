{ genodeEnv, compileSubLibc }:

genodeEnv.mkLibrary {
  name = "libc-locale";
  externalObjects = compileSubLibc {
    sources = [ "lib/libc/locale/*.c" ];
  };
}
