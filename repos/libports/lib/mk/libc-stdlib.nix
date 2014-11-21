{ genodeEnv, compileSubLibc }:

genodeEnv.mkLibrary {
  name = "libc-stdlib";
  externalObjects = compileSubLibc {
    sources = [ "lib/libc/stdlib/*.c" ];
    filter = map
      (fn: "lib/libc/stdlib/${fn}") [ "exit.c" "atexit.c" "malloc.c" ];
  };
}
