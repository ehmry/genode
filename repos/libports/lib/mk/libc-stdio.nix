{ linkStaticLibrary, compileLibc }:

linkStaticLibrary {
  name = "libc-stdio";
  externalObjects = compileLibc {
    sources = [ "lib/libc/stdio/*.c" ];
    localIncludes = [ "lib/libc/locale" "contrib/gdtoa" ];
  };
}
