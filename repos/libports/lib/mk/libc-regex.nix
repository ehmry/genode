{ genodeEnv, compileSubLibc }:

genodeEnv.mkLibrary {
  name = "libc-regex";
  externalObjects = compileSubLibc {
    sources = [ "lib/libc/regex/*.c" ];
    filter =
      [ # 'engine.c' is meant to be included by other compilation
        # units. It cannot be compiled individually.
       "lib/libc/regex/engine.c"
      ];

    localIncludes =
      [ # lib/libc/regex/regcomp.c - lib/libc/locale/collate.h
        "lib/libc/regex"
        "lib/libc/locale"
      ];
  };
}
