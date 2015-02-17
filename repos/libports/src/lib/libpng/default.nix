{ linkSharedLibrary, compileCRepo, newDir, fromDir
, libpngSrc
, libc, libm, zlib }:

let fromDir' = fromDir libpngSrc; in
linkSharedLibrary rec {
  name = "libpng";
  libs = [ libc libm zlib ];

  externalObjects = compileCRepo {
    inherit libs;

    sources = fromDir'
      [ "png.c" "pngset.c" "pngget.c" "pngrutil.c" "pngtrans.c"
        "pngwutil.c" "pngread.c" "pngrio.c" "pngwio.c"
        "pngwrite.c" "pngrtran.c" "pngwtran.c" "pngmem.c"
        "pngerror.c" "pngpread.c"
      ];

    extraFlags =
      [ "-DHAVE_CONFIG_H" "-DPNG_CONFIGURE_LIBPNG" ];

    externalIncludes = [ ../libpng ];
  };

  propagate.externalIncludes =
    [ (newDir "libpng-include" (
        fromDir' [ "pngconf.h" "png.h" "pngpriv.h" ]
      ))
    ];
}
