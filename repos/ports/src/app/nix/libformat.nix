{ linkSharedLibrary, compileCCRepo, nixSrc, stdcxx }:

linkSharedLibrary rec {
  name = "libnixformat";
  libs = [ stdcxx ];
  externalObjects = compileCCRepo {
    inherit name libs;
    sourceRoot = "${nixSrc}/src/boost/format";
    sources = "*.cc";
    systemIncludes = [ "${nixSrc}/src" ];
  };
}
