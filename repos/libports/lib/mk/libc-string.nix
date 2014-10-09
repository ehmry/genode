/*
 * Portion of the string library that is used by both the freestanding string
 * library and the complete libc
 */

{ subLibcEnv }:
let dir = "lib/libc/string"; in
subLibcEnv.mkLibrary {
  name = "libc-string";
  srcSh = [ "${dir}/*.c" ];
  filter = map 
    (fn: "${dir}/${fn}")
    [ "strcoll.c" "strxfrm.c" "wcscoll.c" "wcsxfrm.c" ];
  incDir = [ "${dir}" "include" ];
}
