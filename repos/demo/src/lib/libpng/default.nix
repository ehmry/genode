{ build, base, os, demo }:

build.library {
  name = "libpng_static";
  ccOpt = [ "-funroll-loops" "-DPNG_USER_CONFIG" "-Wno-address" ] ++ build.ccOpt;
  libs = with demo.lib; [ mini_c libz_static ];
  sources = map (fn: (./contrib + "/${fn}"))
    [ "png.c"      "pngerror.c" "pngget.c"   "pngmem.c"
      "pngpread.c" "pngread.c"  "pngrio.c"   "pngrtran.c"
      "pngrutil.c" "pngset.c"   "pngtrans.c" "pngwio.c"
      "pngwrite.c" "pngwtran.c" "pngwutil.c"
    ];

  includeDirs =
    [ (demo.includeDir + "/libpng_static")
      (demo.includeDir + "/libz_static")
      (demo.includeDir + "/mini_c")
    ]
    ++ base.includeDirs;
}