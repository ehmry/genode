{ linkSharedLibrary, compileCRepo, fromDir
, readlineSrc, libc }:

linkSharedLibrary rec {
  name = "history";
  libs = [ libc ];
  propagate.externalIncludes = [ readlineSrc.include ];

  externalObjects = compileCRepo {
    inherit libs;
    externalIncludes =
      [ ../../include/readline
        readlineSrc.source
        readlineSrc.include
      ];
    extraFlags =
      [ "-DHAVE_CONFIG_H" "-DRL_LIBRARY_VERSION=\\\"${readlineSrc.version}\\\""
        # dim build noise for contrib code
        "-Wno-unused-but-set-variable"
      ];
    sources = fromDir
      (toString readlineSrc)
      [ "history.c" "histexpand.c" "histfile.c" "histsearch.c"
        "shell.c" "mbutil.c" "xmalloc.c"
      ];
  };
}