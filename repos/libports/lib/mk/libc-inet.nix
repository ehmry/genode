/*
 * \author Emery Hemingway
 * \date   2014-09-20
 */

{ tool, libc }: with tool;

buildLibrary {
  name = "libc-inet";
  sources =
    filterOut [ ] # need the FILTER_OUT_C
      (wildcard "${libc}/src/lib/libc/lib/libc/inet/*.c");
}