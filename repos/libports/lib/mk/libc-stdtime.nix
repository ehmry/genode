{ linkStaticLibrary, compileSubLibc }:

linkStaticLibrary {
  name = "libc-stdtime";
  externalObjects = compileSubLibc {
    sources = [ "lib/libc/stdtime/*.c" ];
    extraFlags = [ "-DTZ_MAX_TIMES=1" ];
    localIncludes = [ "lib/libc/stdtime" "include" ];
  };
}
