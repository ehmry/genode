/*
 * \author Emery Hemingway
 * \date   2014-09-20
 */

{ tool, libc }: with tool;

buildLibrary {
  name = "libc-regex";
  sources = filterOut 
    # 'engine.c' is meant to be included by other compilation
    # units. It cannot be compiled individually.
    [ "engine.c" ]
    (wildcard "${libc}/src/lib/libc/lib/libc/regex/*.c");
}