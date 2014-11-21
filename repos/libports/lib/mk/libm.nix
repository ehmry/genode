{ genodeEnv, compileSubLibc, libcSrc, libc }:
let
  libmDir = "${libcSrc}/lib/msun";

  archIncludeDir =
    if genodeEnv.isArm    then "${libcSrc}/include/libc-arm"   else
    if genodeEnv.isx86_32 then "${libcSrc}/include/libc-i386"  else
    if genodeEnv.isx86_64 then "${libcSrc}/include/libc-amd64" else
    throw "no libc for ${genodeEnv.system}";
in
genodeEnv.mkLibrary rec {
  name = "libm";
  shared = true;
  libs = [ libc ];

  externalObjects = compileSubLibc {
    inherit name;
    sourceRoot = libmDir;

    # 'e_rem_pio2.c' uses '__inline'
    ccFlags = [ "-D__inline=inline" ] ++ genodeEnv.ccFlags;

    # Work-around to get over doubly defined symbols produced by
    # several sources that include 'e_rem_pio2.c' and 'e_rem_pio2f.c'.
    # To avoid symbol clashes, we rename each occurrence by adding the
    # basename of the compilation unit as suffix.
    ccFlags_s_tanf= [ "-w" ];
    ccFlags_s_tan = [ "-w" ];
    ccFlags_s_sin =
      [ "-w" "-D__ieee754_rem_pio2=__ieee754_rem_pio2_s_sin" ];
    ccFlags_s_cos =
      [ "-w" "-D__ieee754_rem_pio2=__ieee754_rem_pio2_s_cos" ];
    ccFlags_s_cosf=
      [ "-w" "-D__ieee754_rem_pio2f=__ieee754_rem_pio2f_s_cosf"
        "-D__kernel_sindf=__kernel_sindf_cosf"
      ];
    ccFlags_s_sinf =
      [ "-w" "-D__ieee754_rem_pio2f=__ieee754_rem_pio2f_s_sinf"
        "-D__kernel_cosdf=__kernel_cosdf_sinf" ];
    ccFlags_k_cosf = [ "-w" ];
    ccFlags_k_sinf = [ "-w" ];
    ccFlags_k_tanf =
      [ "-w" "-D__ieee754_rem_pio2f=__ieee754_rem_pio2f_s_tanf" ];

    # Disable warnings for selected files, i.e., to suppress
    # 'is static but used in inline function which is not static'
    # messages

    sources =
      [ "src/*.c"
        "ld80/*.c"
        "bsdsrc/*.c"

        # remove on update to version 9
        ../../src/lib/libc/log2.c
      ];

    filter =
      [ "s_exp2l.c"
        "s_lrint.c"
        "s_lround.c"

        # Files that are included by other sources (e.g., 's_sin.c').
        # Hence, we need to remove them from the build. Otherwise,
        # we would end up with doubly defined symbols (and compiler
        # warnings since those files are apparently not meant to be
        # compiled individually).
        "e_rem_pio2.c" "e_rem_pio2f.c"

        "log2.c" # remove on update to version 9
      ];

    localIncludes =
      (map (d: "${libmDir}/${d}")
        [ "src" # finding 'math_private.h'
          "ld80" # finding 'invtrig.h', included by 'e_acosl.c'
          "bsdsrc" # finding 'mathipml.h', included by 'b_exp.c'
        ]
      ) ++
      (map (d: "${libcSrc}/${d}")
        [ # finding 'fpmath.h', included by 'invtrig.h'
          "lib/libc/include"
        ]
      ) ++ [ archIncludeDir ];

      systemIncludes = [ "${libcSrc}/include/libc" archIncludeDir ];

  };

}
