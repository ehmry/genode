{ linkStaticLibrary, compileLibc }:

linkStaticLibrary {
  name = "libc-isc";
  externalObjects = compileLibc {
    sources = [ "lib/libc/isc/*.c" ];
    systemIncludes = [ "sys" ];
  };
}
