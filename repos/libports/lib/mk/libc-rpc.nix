{ linkStaticLibrary, compileLibc, fromLibc }:

linkStaticLibrary {
  name = "libc-rpc";
  externalObjects = compileLibc {
    sources = [ "lib/libc/rpc/bindresvport.c" ];
    externalIncludes = fromLibc [ "sys/rpc" "sys" ];
  };
}
