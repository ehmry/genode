{ linkSharedLibrary, compileCCRepo, compileCRepo, nixSrc, libc, stdcxx }:
let sourceRoot = "${nixSrc}/src/libutil"; in
linkSharedLibrary rec {
  name = "libnixutil";
  # Danger! Both these libraries have a regex.h.
  libs = [ libc stdcxx ];
  externalObjects = 
    [ (compileCCRepo {
        inherit name libs sourceRoot;
        sources = "*.cc";
        systemIncludes = [ ./include "${nixSrc}/src" ];
      })

      # We are not providing an OpenSSL library.
      (compileCRepo {
        inherit name libs sourceRoot;
        sources = "md5.c sha1.c sha256.c";
      })
  ];

  propagate.systemIncludes = [ sourceRoot "${nixSrc}/src" ];
}
