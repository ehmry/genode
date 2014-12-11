{ linkStaticLibrary, compileLibc }:

linkStaticLibrary {
  name = "libc-rpc";
  externalObjects = compileLibc {
    sources = [ "lib/libc/rpc/bindresvport.c" ];
    systemIncludes = [ "sys/rpc" "sys" ];
  };
}
