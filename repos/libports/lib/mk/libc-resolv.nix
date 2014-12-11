{ linkStaticLibrary, compileLibc }:

linkStaticLibrary {
  name = "libc-resolv";
  externalObjects = compileLibc {
    sources = [ "lib/libc/resolv/*.c" ];
  };
}
