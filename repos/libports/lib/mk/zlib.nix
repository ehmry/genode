{ linkSharedLibrary, compileCRepo, fromDir, newDir
, zlibSrc, libc }:

let fromDir' = fromDir zlibSrc; in
linkSharedLibrary rec {
  name = "zlib";
  libs = [ libc ];
  externalObjects = compileCRepo {
    inherit libs;
    sources = fromDir'
      [ "adler32.c" "compress.c" "crc32.c" "deflate.c" "gzclose.c"
        "gzlib.c" "gzread.c" "gzwrite.c" "infback.c" "inffast.c"
        "inflate.c" "inftrees.c" "trees.c" "uncompr.c" "zutil.c"
      ];
  };

  propagate.externalncludes =
    [ (newDir "zlib-include" (fromDir' [ "zconf.h" "zlib.h" ])) ];
}
