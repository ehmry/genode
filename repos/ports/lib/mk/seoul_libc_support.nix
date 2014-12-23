{ linkStaticLibrary, compileLibc, libcSrc, libc }:

linkStaticLibrary {
  name = "seoul_libc_support";
  externalObjects = compileLibc {
    sourceRoot = libcSrc + "/lib/libc";
    sources =
      [ "stdlib/strtoul.c"
        "sys/__error.c" "gen/errno.c"
        "locale/none.c" "locale/table.c"
      ] ++
      (map
        (s:"string/"+s)
        [ "strchr.c" "strncpy.c" "strspn.c" "strcspn.c"
          "strstr.c" "strlen.c" "strnlen.c" "strcpy.c"
          "memcmp.c" "strcmp.c"
        ]
      );
  };

  propagatedIncludes =
    [ "${libcSrc.include}/libc"
    ] ++ libc.propagatedIncludes;
}
