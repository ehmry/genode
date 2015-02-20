{ linkStaticLibrary, compileC, fromDir, mini_c }:
let externalIncludes = [ ../../../include/libz_static ]; in
linkStaticLibrary {
  name = "libz_static";
  objects = map
    (src: compileC {
      inherit src;
      libs = [ mini_c ]; # Just need the headers to compile, not link.
      externalIncludes = [ ./contrib ] ++ externalIncludes;
    })
    (fromDir ./contrib
      [ "adler32.c"  "compress.c" "crc32.c"   "deflate.c"
        "gzio.c"     "infback.c"  "inffast.c" "inflate.c"
        "inftrees.c" "trees.c"    "uncompr.c" "zutil.c"
      ]
    );
  propagate = { inherit externalIncludes; };
}
