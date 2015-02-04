{ linkStaticLibrary, compileLibc, fromLibc }:

linkStaticLibrary {
  name = "libc-stdio";
  externalObjects = compileLibc {
    sources = [ "lib/libc/stdio/*.c" ];
    externalIncludes = fromLibc [ "lib/libc/locale" "contrib/gdtoa" ];
  };
}
