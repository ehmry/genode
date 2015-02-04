{ linkStaticLibrary, compileC, mini_c, libz_static }:
let
  libs = [ mini_c libz_static ];

  compileC' = src: compileC {
    inherit src libs;
    extraFlags =
      [ "-funroll-loops" "-DPNG_USER_CONFIG" "-Wno-address" ];
    includes = [ ../../../include/libpng_static ];
  };
in
linkStaticLibrary {
  name = "libpng_static";
  inherit libs;
  objects = map (fn: compileC' (./contrib + "/${fn}"))
    [ "png.c"      "pngerror.c" "pngget.c"   "pngmem.c"
      "pngpread.c" "pngread.c"  "pngrio.c"   "pngrtran.c"
      "pngrutil.c" "pngset.c"   "pngtrans.c" "pngwio.c"
      "pngwrite.c" "pngwtran.c" "pngwutil.c"
    ];
}
