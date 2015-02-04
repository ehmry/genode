/*
 * Portion of the string library that is used by both the freestanding string
 * library and the complete libc
 */

{ linkStaticLibrary, addPrefix, compileLibc }:

let dir = "lib/libc/string/"; in
linkStaticLibrary {
  name = "libc-string";

  externalObjects = compileLibc {
    sources = [ "${dir}*.c" ];
    filter = addPrefix dir
      [ "strcoll.c" "strxfrm.c" "wcscoll.c" "wcsxfrm.c" ];
  };
}
