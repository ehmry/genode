{ subLibcEnv }:
subLibcEnv.mkLibrary {
  name = "libc-regex";
  sourceSh = [ "lib/libc/regex/*.c" ];
  filter =
    [ # 'engine.c' is meant to be included by other compilation
      # units. It cannot be compiled individually.
      "lib/libc/regex/engine.c"
    ];

  localIncludesSh =
    [ # lib/libc/regex/regcomp.c - lib/libc/locale/collate.h
      "lib/libc/locale/"
    ];
}
