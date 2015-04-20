{ linkSharedLibrary, compileCC, compileCCRepo, fromDir
, nixSrc, libc, stdcxx, nixformat, nixstore, nixutil }:

let
  sourceRoot = "${nixSrc}/src/libexpr";
  externalIncludes = [ sourceRoot ];


  # Make sure libc's regex.h comes before stdcxx's regex.h.
  extraFlags = map (i: "-I"+i) libc.propagate.externalIncludes;
in
linkSharedLibrary rec {
  name = "nixexpr";
  libs = [ libc stdcxx nixformat nixstore nixutil ];
  externalObjects = compileCCRepo {
    inherit libs externalIncludes extraFlags;
    sources = fromDir
      sourceRoot
      [ "attr-path.cc"
        "common-opts.cc"
        "eval.cc"
        "get-drvs.cc"
        "json-to-value.cc"
        "lexer-tab.cc"
        "names.cc"
        "nixexpr.cc"
        "parser-tab.cc"
        "primops.cc"
        "value-to-json.cc"
        "value-to-xml.cc"
      ];
  };

  propagate = { inherit extraFlags; };
}
