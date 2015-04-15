{ linkSharedLibrary, compileCCRepo, addPrefix, fromDir, nixSrc, stdcxx }:

linkSharedLibrary rec {
  name = "nixformat";
  libs = [ stdcxx ];
  externalObjects = compileCCRepo {
      inherit libs;
      externalIncludes = [ "${nixSrc}/src" ];
      sources = fromDir
        "${nixSrc}/src/boost/format"
        [ "format_implementation.cc"
          "free_funcs.cc" "parsing.cc"
        ];
  };
}
