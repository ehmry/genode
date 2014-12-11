{ linkStaticLibrary, compileLibc }:
linkStaticLibrary {
  name = "libc-inet";
  externalObjects = compileLibc {
    sources = [ "lib/libc/inet/*.c" ];
  };
}
