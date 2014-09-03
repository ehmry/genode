{ build, base, os, demo }:

build.library {
  name = "libz_static";
  sources = map (fn: (./contrib + "/${fn}"))
    [ "adler32.c"  "compress.c" "crc32.c"   "deflate.c"
      "gzio.c"     "infback.c"  "inffast.c" "inflate.c"
      "inftrees.c" "trees.c"    "uncompr.c" "zutil.c"
    ];

  includeDirs =
    [ ./contrib
      (demo.includeDir + "/libz_static")
      (demo.includeDir + "/mini_c")
    ] ++ base.includeDirs;
}