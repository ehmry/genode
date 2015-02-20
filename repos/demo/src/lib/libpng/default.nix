{ linkStaticLibrary, compileC, fromDir, mini_c, libz_static }:

let externalIncludes = [ ../../../include/libpng_static ]; in
linkStaticLibrary rec {
  name = "libpng_static";
  libs = [ mini_c libz_static ];
  objects = map
    (src: compileC {
      inherit src libs externalIncludes;
      extraFlags =
        [ "-funroll-loops" "-DPNG_USER_CONFIG" "-Wno-address" ];
    })
    (fromDir ./contrib
      [ "png.c"      "pngerror.c" "pngget.c"   "pngmem.c"
        "pngpread.c" "pngread.c"  "pngrio.c"   "pngrtran.c"
        "pngrutil.c" "pngset.c"   "pngtrans.c" "pngwio.c"
        "pngwrite.c" "pngwtran.c" "pngwutil.c"
      ]
    );
  propagate = { inherit externalIncludes; };
}
