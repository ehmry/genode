{ linkStaticLibrary, compileLibc }:

linkStaticLibrary {
  name = "libc-nameser";
  externalObjects = compileLibc {
    sources = [ "lib/libc/nameser/*.c" ];
  };
}
