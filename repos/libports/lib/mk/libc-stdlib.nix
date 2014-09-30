/*
 * \author Emery Hemingway
 * \date   2014-09-20
 */

{ tool, libc }: with tool;

buildLibrary {
  name = "libc-stdlib";
  sources =
    filterOut [ "exit.c" "atexit.c" "malloc.c" ]
      (wildcard "${libc}/src/lib/libc/lib/libc/stdlib/*.c");
}