{ genodeEnv, linkStaticLibrary, addPrefix, compileLibc, fromLibc }:

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
        filter = addPrefix "lib/libc/i386/gen/"
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
        filter = addPrefix "$lib/libc/amd64/gen/"
          [ "rfork_thread.S" "setjmp.S" "_setjmp.S" "_set_tp.c"

            # Filter functions conflicting with libm
            "fabs.S" "modf.S" "frexp.c"
          ];
      }
    else throw "incomplete libc-gen expression for ${genodeEnv.system}";

in
linkStaticLibrary rec {
  name = "libc-gen";

  externalObjects = compileLibc (genodeEnv.tool.mergeSet archArgs {
    inherit name;
    sources = [ "${genDir}/*.c" ];

    filter = addPrefix "${genDir}/"
      # this file produces a warning about a missing header file,
      # lets drop it
      [ "getosreldate.c" "sem.c" "valloc.c" "getpwent.c" ];

      externalIncludes = fromLibc [ "sys" "lib/libc/locale" ];
    });

}
