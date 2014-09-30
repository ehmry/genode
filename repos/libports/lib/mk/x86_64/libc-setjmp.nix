/*
 * \author Emery Hemingway
 * \date   2014-09-21
 */

{ tool, libc }: with tool;

buildLibrary {
  name = "libc-setjmp";
  sources = fromDir 
    "${libc}/src/lib/libc/lib/libc/amd64/gen/"
    [ "_setjmp.S" "setjmp.S" ];
}