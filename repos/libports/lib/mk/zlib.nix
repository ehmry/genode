{ genodeEnv, linkSharedLibrary, compileCRepo
, zlibSrc
, libc }:

with genodeEnv.tool;

linkSharedLibrary rec {
  name = "zlib";
  libs = [ libc ];
  externalObjects = compileCRepo {
    inherit libs;
    sourceRoot = zlibSrc;
    sources =
      [ "adler32.c" "compress.c" "crc32.c" "deflate.c" "gzclose.c"
        "gzlib.c" "gzread.c" "gzwrite.c" "infback.c" "inffast.c"
        "inflate.c" "inftrees.c" "trees.c" "uncompr.c" "zutil.c"
      ];
  };

  propagatedIncludes =
    [ (newDir "zlib-include" (
        map (fn: "${zlibSrc}/${fn}") [ "zconf.h" "zlib.h" ]
      ))
    ];
}
