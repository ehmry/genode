{ linkSharedLibrary, compileCC, compileCCRepo, compileCRepo, fromDir
, nixSrc, libc, stdcxx }:

let
  sourceRoot = "${nixSrc}/src/libutil"; 
  includes = [ ../../../include/nix ];
  headers = [ "${sourceRoot}/types.hh" ];
in
linkSharedLibrary rec {
  name = "nixutil";
  # Danger! Both these libraries have a regex.h.
  libs = [ libc stdcxx ];

  objects = compileCC {
    src = ./util-genode.cc;
    inherit libs includes headers;
    externalIncludes = [ "${nixSrc}/src" ];
  };

  externalObjects =
    [ (compileCCRepo {
        externalIncludes = [ sourceRoot "${nixSrc}/src" ];
        inherit libs includes headers;
        sources = "${sourceRoot}/*.cc";
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
