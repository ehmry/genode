{ linkStaticLibrary, compileLibc }:

linkStaticLibrary {
  name = "libc-stdtime";
  externalObjects = compileLibc {
    sources = [ "lib/libc/stdtime/*.c" ];
    extraFlags = [ "-DTZ_MAX_TIMES=1" ];
    localIncludes = [ "lib/libc/locale" ];
  };
}
