{ genodeEnv, compileSubLibc }:

genodeEnv.mkLibrary {
  name = "libc-compat";
  externalObjets = compileSubLibc {
    sources = [ "lib/libc/compat-43/*.c" ];
  };
}
