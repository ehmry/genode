{ linkSharedLibrary, compileCRepo, fromDir, newDir
, zlibSrc, libc }:

let
  fromDir' = fromDir zlibSrc;
  externalIncludes =
    [ (newDir "zlib-include" (fromDir' [ "zconf.h" "zlib.h" ])) ];
in
linkSharedLibrary rec {
  name = "zlib";
  libs = [ libc ];
  externalObjects = compileCRepo {
    inherit libs externalIncludes;
    sources = fromDir'
      [ "adler32.c" "compress.c" "crc32.c" "deflate.c" "gzclose.c"
        "gzlib.c" "gzread.c" "gzwrite.c" "infback.c" "inffast.c"
        "inflate.c" "inftrees.c" "trees.c" "uncompr.c" "zutil.c"
      ];
  };

  propagate = { inherit externalIncludes; };
}
