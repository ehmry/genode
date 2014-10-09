{ subLibcEnv }:
subLibcEnv.mkLibrary {
  name = "libc-stdtime";
  ccOpt = [ "-DTZ_MAX_TIMES=1" ];
  srcSh = [ "lib/libc/stdtime/*.c" ];
  incDir = [ "lib/libc/stdtime" ];

  preGather = "touch libintl.h";
}
