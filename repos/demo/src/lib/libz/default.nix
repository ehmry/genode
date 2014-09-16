{ build }:
{ }:

build.library {
  name = "libz_static";
  sources = map (fn: (./contrib + "/${fn}"))
    [ "adler32.c"  "compress.c" "crc32.c"   "deflate.c"
      "gzio.c"     "infback.c"  "inffast.c" "inflate.c"
      "inftrees.c" "trees.c"    "uncompr.c" "zutil.c"
    ];

  includeDirs =
    [ ./contrib
      ../../../include/libz_static
      ../../../include/mini_c
    ];
}