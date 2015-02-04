{ linkStaticLibrary, compileLibc }:

linkStaticLibrary {
  name = "libc-rpc";
  externalObjects = compileLibc {
    sources = [ "lib/libc/rpc/bindresvport.c" ];
    externalIncludes = [ "sys/rpc" "sys" ];
  };
}
