{ linkStaticLibrary, addPrefix, compileLibc, libcSrc }:

linkStaticLibrary {
  name = "libc-gdtoa";
  externalObjects = compileLibc {
    sources = [ "contrib/gdtoa/*.c" "lib/libc/gdtoa/*.c" ];
    filter =
      (addPrefix "contrib/gdtoa/"
        [ "arithchk.c" "strtodnrp.c" "qnan.c" ]
      ) ++
      (addPrefix "lib/libc/gdtoa/"
        [ "machdep_ldisQ.c" "machdep_ldisx.c" ]
      );
    externalIncludes = [ "${libcSrc}/contrib/gdtoa" ];
  };
}
