{ linkStaticLibrary, compileC, mini_c }:

let
  compileC' = src: compileC {
    inherit src;
    localIncludes = [ ./contrib ../../../include/libz_static ];
    systemIncludes = [ ./contrib ];
    libs = [ mini_c ]; # Just need the headers to compile, not link.
  };
in
linkStaticLibrary {
  name = "libz_static";
  objects = map (fn: compileC' (./contrib + "/${fn}"))
    [ "adler32.c"  "compress.c" "crc32.c"   "deflate.c"
      "gzio.c"     "infback.c"  "inffast.c" "inflate.c"
      "inftrees.c" "trees.c"    "uncompr.c" "zutil.c"
    ];
  propagate.systemIncludes = [ ../../../include/libz_static ];
}
