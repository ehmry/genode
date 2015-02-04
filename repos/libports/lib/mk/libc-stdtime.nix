{ linkStaticLibrary, compileLibc, libcSrc }:

linkStaticLibrary {
  name = "libc-stdtime";
  externalObjects = compileLibc {
    sources = [ "lib/libc/stdtime/*.c" ];
    extraFlags = [ "-DTZ_MAX_TIMES=1" ];
    externalIncludes = [ "${libcSrc}/lib/libc/locale" ];
  };
}
