{ linkStaticLibrary, compileCRepo, bzip2Src, libc }:

linkStaticLibrary rec {
  name = "libbz2";
  libs = [ libc ];
  externalObjects = compileCRepo {
    inherit name libs;
    extraFlags = [ "-Winline" "-D_FILE_OFFSET_BITS=64" ];
    sourceRoot = bzip2Src;
    sources = 
      [ "blocksort.c" "huffman.c" "crctable.c" "randtable.c"
        "compress.c" "decompress.c" "bzlib.c"
      ];
  };
  propagate.systemIncludes = [ bzip2Src.include ];
}
