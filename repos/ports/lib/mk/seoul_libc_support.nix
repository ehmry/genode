{ linkStaticLibrary, compileLibc, addPrefix, libcSrc, libc }:

linkStaticLibrary {
  name = "seoul_libc_support";
  externalObjects = compileLibc {
    sourceRoot = libcSrc + "/lib/libc";
    sources = addPrefix "${libcSrc}/lib/libc/"
      [ "stdlib/strtoul.c"
        "sys/__error.c" "gen/errno.c"
        "locale/none.c" "locale/table.c"
      ] ++
      (addPrefix "string/"
        [ "strchr.c" "strncpy.c" "strspn.c" "strcspn.c"
          "strstr.c" "strlen.c" "strnlen.c" "strcpy.c"
          "memcmp.c" "strcmp.c"
        ]
      );
  };

  propagate.externalncludes =
    [ "${libcSrc.include}/libc"
    ] ++ libc.propagate.externalIncludes;
}
