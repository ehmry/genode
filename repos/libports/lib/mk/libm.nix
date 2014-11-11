{ libcEnv, libcSrc, libc }:
let
  libmDir = "${libcSrc}/lib/msun";
in
libcEnv.mkLibrary {
  name = "libm";
  shared = true;
  libs = [ libc ];

  sourceRoot = libmDir;

  # 'e_rem_pio2.c' uses '__inline'
  ccOpt = libcEnv.ccOpt ++ [ "-D__inline=inline" ];

  # Work-around to get over doubly defined symbols produced by several sources
  # that include 'e_rem_pio2.c' and 'e_rem_pio2f.c'. To avoid symbol clashes,
  # we rename each occurrence by adding the basename of the compilation unit
  # as suffix.
  ccOpt_s_tanf= [ "-w" ];
  ccOpt_s_tan = [ "-w" ];
  ccOpt_s_sin = [ "-w" "-D__ieee754_rem_pio2=__ieee754_rem_pio2_s_sin" ];
  ccOpt_s_cos = [ "-w" "-D__ieee754_rem_pio2=__ieee754_rem_pio2_s_cos" ];
  ccOpt_s_cosf= [ "-w" "-D__ieee754_rem_pio2f=__ieee754_rem_pio2f_s_cosf" "-D__kernel_sindf=__kernel_sindf_cosf" ];
  ccOpt_s_sinf= [ "-w" "-D__ieee754_rem_pio2f=__ieee754_rem_pio2f_s_sinf" "-D__kernel_cosdf=__kernel_cosdf_sinf" ];
  ccOpt_k_cosf= [ "-w" ];
  ccOpt_k_sinf= [ "-w" ];
  ccOpt_k_tanf= [ "-w" "-D__ieee754_rem_pio2f=__ieee754_rem_pio2f_s_tanf" ];

  # Disable warnings for selected files, i.e., to suppress
  # 'is static but used in inline function which is not static'
  # messages

  sourceSh =
    [ "src/*.c"
      "ld80/*.c"
      "bsdsrc/*.c"
    ];

  # remove on update to version 9
  sources = libcEnv.fromPath ../../src/lib/libc/log2.c;

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

  localIncludesSh = [
    # finding 'math_private.h'
    "${libmDir}/src"

    # finding 'invtrig.h', included by 'e_acosl.c'
    "${libmDir}/ld80"

    # finding 'mathipml.h', included by 'b_exp.c'
    "${libmDir}/bsdsrc"

    # finding 'fpmath.h', included by 'invtrig.h'
    "${libcSrc}/lib/libc/include"
  ];

}
