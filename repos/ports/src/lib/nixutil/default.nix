{ linkSharedLibrary, compileCC, compileCCRepo, compileCRepo, fromDir
, nixSrc, libc, stdcxx }:

let
  sourceRoot = "${nixSrc}/src/libutil"; 
  includes = [ ../../../include/nix ];
  headers = [ "${sourceRoot}/types.hh" ];
  externalIncludes = [ sourceRoot "${nixSrc}/src" ];
in
linkSharedLibrary rec {
  name = "nixutil";
  # Danger! Both these libraries have a regex.h.
  libs = [ libc stdcxx ];

  objects = compileCC {
    src = ./util.cc;
    inherit libs includes headers externalIncludes;
  };

  externalObjects =
    [ (compileCCRepo {
        inherit libs includes headers externalIncludes;
        sources = fromDir
          sourceRoot
          [ "affinity.cc"
            "archive.cc"
            "hash.cc"
            "regex.cc"
            "serialise.cc"
            "xml-writer.cc"
          ];
      })

      # We are not providing an OpenSSL library.
      (compileCRepo {
        inherit libs;
        sources = fromDir sourceRoot [ "md5.c" "sha1.c" "sha256.c" ];
      })
    ];

  propagate.externalIncludes =
    [ ../../../include/nix sourceRoot "${nixSrc}/src" ];
}
