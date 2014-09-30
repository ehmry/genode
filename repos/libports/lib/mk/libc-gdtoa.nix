/*
 * \author Emery Hemingway
 * \date   2014-09-20
 */

{ tool, libc }: with tool;

buildLibrary {
  name = "libc-gdtoa";
  sources = 
    filterOut [ "arithchk.c" "strtodnrp.c" "qnan.c" "machdep_ldisQ.c" "machdep_ldisx.c" ]
      ( (wildcard "${libc}/src/lib/libc/contrib/gdtoa/*.c")
        ++
        (wildcard "${libc}/src/lib/libc/lib/libc/gdtoa/*.c")
      );
}