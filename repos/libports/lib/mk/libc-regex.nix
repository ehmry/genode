{ linkStaticLibrary, compileLibc, fromLibc }:

linkStaticLibrary {
  name = "libc-regex";
  externalObjects = compileLibc {
    sources = [ "lib/libc/regex/*.c" ];
    filter =
      [ # 'engine.c' is meant to be included by other compilation
        # units. It cannot be compiled individually.
       "lib/libc/regex/engine.c"
      ];

    externalIncludes = fromLibc
      [ # lib/libc/regex/regcomp.c - lib/libc/locale/collate.h
        "lib/libc/regex"
        "lib/libc/locale"
      ];
  };
}
