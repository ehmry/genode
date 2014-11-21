{ genodeEnv, compileSubLibc }:

let
  repoSrcDir = ../../src/lib/libc;

  genDir = "lib/libc/gen";

  archArgs =
    if genodeEnv.isx86_32 then
      { sources =
          [ "lib/libc/i386/gen/*.S"
            "lib/libc/i386/gen/flt_rounds.c"
            "lib/libc/i386/gen/makecontext.c"
          ];
        filter = map (fn: "lib/libc/i386/gen/${fn}")
          [ "rfork_thread.S" "setjmp.S" "_setjmp.S" "_set_tp.c"

            # Filter functions conflicting with libm
            "fabs.S" "modf.S" "frexp.c"
          ];
      }
    else
    if genodeEnv.isx86_64 then
      { sources =
          [ "lib/libc/amd64/gen/*.S"
            "lib/libc/amd64/gen/flt_rounds.c"
          ];
        filter = map (fn: "lib/libc/amd64/gen/${fn}")
          [ "rfork_thread.S" "setjmp.S" "_setjmp.S" "_set_tp.c"

            # Filter functions conflicting with libm
            "fabs.S" "modf.S" "frexp.c"
          ];
      }
    else throw "incomplete libc-gen expression for ${genodeEnv.system}";

in
genodeEnv.mkLibrary rec {
  name = "libc-gen";

  externalObjects = compileSubLibc (genodeEnv.tool.mergeSet archArgs {
    inherit name;
    sources = [ "${genDir}/*.c" ];

    filter = map (fn: "${genDir}/${fn}")
      # this file produces a warning about a missing header file,
      # lets drop it
      [ "getosreldate.c" "sem.c" "valloc.c" "getpwent.c" ];

      localIncludes =
        [ ( if genodeEnv.isx86_32 then "include/libc-i386"  else
            if genodeEnv.isx86_64 then "include/libc-amd64" else
            if genodeEnv.isArm    then "include/libc-arm"    else
            throw "no libc for ${genodeEnv.system}"
          )
        ];

      systemIncludes =
        [ # libc_pdbg.h
          ../../src/lib/libc
        ];
    });

}
