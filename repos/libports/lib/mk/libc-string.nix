/*
 * \author Emery Hemingway
 * \date   2014-09-19
 *
 * Portion of the string library that is used by both the freestanding string
 * library and the complete libc
 */

{ tool, libc }: with tool;

buildLibrary {
  name = "libc-string";

  # These files would infect the freestanding
  # string library with the locale library
  sources = tool.filterOut
    [ "strcoll.c" "strxfrm.c" "wcscoll.c" "wcsxfrm.c" ]
    (tool.wildcard "${libc}/src/lib/libc/lib/libc/string/*.c");
}