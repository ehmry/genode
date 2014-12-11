{ linkStaticLibrary, compileLibc }:

linkStaticLibrary {
  name = "libc-locale";
  externalObjects = compileLibc {
    sources = [ "lib/libc/locale/*.c" ];
  };
}
