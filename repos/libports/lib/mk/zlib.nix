/*
 * \author Emery Hemingway
 * \date   2014-09-30
 */

{ tool }: with tool;
{ zlib }:
{ libc }:

buildLibrary {
  name = "zlib";
  shared = true;
  libs = [ libc ];
  sources = fromDir zlib [
    "adler32.c" "compress.c" "crc32.c" "deflate.c" "gzclose.c"
    "gzlib.c" "gzread.c" "gzwrite.c" "infback.c" "inffast.c"
    "inflate.c" "inftrees.c" "trees.c" "uncompr.c" "zutil.c"
  ];
  includeDirs = [ ];
  propagatedIncludes =
    [ (newDir "zlib-include" (
        fromDir zlib [ "zconf.h" "zlib.h" ]) )
  ];
}