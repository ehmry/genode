{ tool }:
{ mini_c, libz_static }:

with tool;

buildLibrary {
  name = "libpng_static";
  ccOpt = [ "-funroll-loops" "-DPNG_USER_CONFIG" "-Wno-address" ] ++ build.ccOpt;
  libs = [ mini_c libz_static ];
  sources = fromDir ./contrib
    [ "png.c"      "pngerror.c" "pngget.c"   "pngmem.c"
      "pngpread.c" "pngread.c"  "pngrio.c"   "pngrtran.c"
      "pngrutil.c" "pngset.c"   "pngtrans.c" "pngwio.c"
      "pngwrite.c" "pngwtran.c" "pngwutil.c"
    ];

  includeDirs =
    [ ../../../include/libpng_static
      ../../../include/libz_static
      ../../../include/mini_c
    ];

}