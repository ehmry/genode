{ subLibcEnv }:

let
  repoSrcDir = ../../src/lib/libc;

  genDir = "lib/libc/gen";

  archArgs =
    if subLibcEnv.isx86_64 then {
      sourceSh =
        [ "lib/libc/amd64/gen/*.S"
          "lib/libc/amd64/gen/flt_rounds.c"
        ];
      filter = map (fn: "lib/libc/amd64/gen/${fn}")
        [ "rfork_thread.S" "setjmp.S" "_setjmp.S" "_set_tp.c"

          # Filter functions conflicting with libm
          "fabs.S" "modf.S" "frexp.c"
        ];
    } else throw "no libc-gen for ${subLibcEnv.system}";

in
subLibcEnv.mkLibrary (subLibcEnv.tool.mergeSet archArgs {
  name = "libc-gen";
  sourceSh = [ "${genDir}/*.c" ];

  filter = map (fn: "${genDir}/${fn}")
    # this file produces a warning about a missing header file,
    # lets drop it
    [ "getosreldate.c" "sem.c" "valloc.c" "getpwent.c" ];

    localIncludesSh =
      [ ( if subLibcEnv.isx86_32 then "include/libc-i386"  else
          if subLibcEnv.isx86_64 then "include/libc-amd64" else
          if subLibcEnv.isxarm then  "include/libc-arm"    else
          throw "no libc for ${subLibcEnv.system}"
        )
      ];

    systemIncludes =
      [ # libc_pdbg.h
        ../../src/lib/libc
      ];

})
