{ subLibcEnv }:

let
  repoSrcDir = ../../src/lib/libc;

  genDir = "lib/libc/gen";

  archArgs =
    if subLibcEnv.isx86_64 then {
      srcSh =
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
  srcSh = [ "${genDir}/*.c" ];

  filter = map (fn: "${genDir}/${fn}")
    # this file produces a warning about a missing header file,
    # lets drop it
    [ "getosreldate.c" "sem.c" "valloc.c" "getpwent.c" ];

# getpwent.c is not being filtered out

  # Create fake headers
  preGather = ''
    mkdir -p machine mach
    touch machine/pctr.h mach/port.h hesiod.h
  '';

  incDir =
    [ genDir
      "lib/libc/stdtime"

      # libc_pdbg.h
      ../../src/lib/libc
    ];
})
