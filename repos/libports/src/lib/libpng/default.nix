{ genodeEnv, linkSharedLibrary, compileCRepo
, libpngSrc
, libc, libm, zlib }:

with genodeEnv.tool;

linkSharedLibrary rec {
  name = "libpng";
  libs = [ libc libm zlib ];

  externalObjects = compileCRepo {
    inherit libs;
    sourceRoot = libpngSrc;

    sources =
      [ "png.c" "pngset.c" "pngget.c" "pngrutil.c" "pngtrans.c"
        "pngwutil.c" "pngread.c" "pngrio.c" "pngwio.c"
        "pngwrite.c" "pngrtran.c" "pngwtran.c" "pngmem.c"
        "pngerror.c" "pngpread.c"
      ];

    extraFlags =
      [ "-DHAVE_CONFIG_H" "-DPNG_CONFIGURE_LIBPNG" ];

    systemIncludes = [ (genodeEnv.tool.filterHeaders ../libpng) ];
  };

  propagate.systemIncludes =
    [ (newDir "libpng-include" (
        fromDir libpngSrc [ "pngconf.h" "png.h" "pngpriv.h" ]
      ))
    ];
}
