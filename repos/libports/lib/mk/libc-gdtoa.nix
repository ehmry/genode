{ subLibcEnv }:
subLibcEnv.mkLibrary {
  name = "libc-gdtoa";
  srcSh = [ "contrib/gdtoa/*.c" "lib/libc/gdtoa/*.c" ];
  filter = (
    map (fn: "contrib/gdtoa/${fn}")
      [ "arithchk.c" "strtodnrp.c" "qnan.c" ]
    ++
    map (fn: "lib/libc/gdtoa/${fn}")
      [ "machdep_ldisQ.c" "machdep_ldisx.c" ]
  );
}
