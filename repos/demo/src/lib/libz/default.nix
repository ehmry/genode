{ genodeEnv, compileC }:

let
  compileC' = src: compileC {
    inherit src;
    localIncludes = [ ./contrib ../../../include/libz_static ];
     systemIncludes = [ ./contrib ../../../include/mini_c ];
  };
in
genodeEnv.mkLibrary {
  name = "libz_static";
  objects = map (fn: compileC' (./contrib + "/${fn}"))
    [ "adler32.c"  "compress.c" "crc32.c"   "deflate.c"
      "gzio.c"     "infback.c"  "inffast.c" "inflate.c"
      "inftrees.c" "trees.c"    "uncompr.c" "zutil.c"
    ];
  #propagatedIncludes = [ ../../../include/libz_static ];
}
