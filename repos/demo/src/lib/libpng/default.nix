{ genodeEnv, compileC, mini_c, libz_static }:
let
  compileC' = src: compileC {
    inherit src;
    extraFlags =
      [ "-funroll-loops" "-DPNG_USER_CONFIG" "-Wno-address" ];
    localIncludes =
      [ ../../../include/libpng_static
        ../../../include/libz_static
      ];
    systemIncludes = [ ../../../include/mini_c ];
  };
in
genodeEnv.mkLibrary {
  name = "libpng_static";
  libs = [ mini_c libz_static ];

  objects = map (fn: compileC' (./contrib + "/${fn}"))
    [ "png.c"      "pngerror.c" "pngget.c"   "pngmem.c"
      "pngpread.c" "pngread.c"  "pngrio.c"   "pngrtran.c"
      "pngrutil.c" "pngset.c"   "pngtrans.c" "pngwio.c"
      "pngwrite.c" "pngwtran.c" "pngwutil.c"
    ];
}
