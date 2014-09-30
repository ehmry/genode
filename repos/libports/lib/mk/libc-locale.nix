/*
 * \author Emery Hemingway
 * \date   2014-09-20
 */

{ tool, libc }: with tool;

buildLibrary {
  name = "libc-locale";
  sources = wildcard "${libc}/src/lib/libc/lib/libc/locale/*.c";
}