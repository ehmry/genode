{ linkStaticLibrary, compileLibc, addPrefix }:

linkStaticLibrary {
  name = "libc-stdlib";
  externalObjects = compileLibc {
    sources = [ "lib/libc/stdlib/*.c" ];
    filter = addPrefix
      "lib/libc/stdlib/"
      [ "exit.c" "atexit.c" "malloc.c" ];
  };
}
