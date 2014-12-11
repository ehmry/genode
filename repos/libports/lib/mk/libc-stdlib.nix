{ linkStaticLibrary, compileLibc }:

linkStaticLibrary {
  name = "libc-stdlib";
  externalObjects = compileLibc {
    sources = [ "lib/libc/stdlib/*.c" ];
    filter = map
      (fn: "lib/libc/stdlib/${fn}") [ "exit.c" "atexit.c" "malloc.c" ];
  };
}
