/*
 * \author Emery Hemingway
 * \date   2014-09-20
 */

{ tool, libc }: with tool;

buildLibrary {
  name = "libc-stdtime";
  ccOpt = [ "-DTZ_MAX_TIMES=1" ];
  sources = tool.wildcard "${libc}/src/lib/libc/lib/libc/stdtime/*.c";
}