{ linkStaticLibrary, compileC, compileCRepo, shellDerivation
, genodeEnv, spec, nixpkgs, addPrefix
, gmpSrc
, libc }:

let
  mpnDir = "${gmpSrc}/mpn";
  gmpInclude = ../../../include/gmp;
  arch =
    if spec.isx86_64 then "x86_64" else
    throw "${spec.system} not supported by ${gmpSrc.name}";

  bits =
    if spec.is32Bit then "32bit" else
    if spec.is64Bit then "64bit" else
    throw "${spec.system} not supported by ${gmpSrc.name}";

  m4EnvSetup =
    ''
      mkdir -p mpn/${arch}
      ln -s ${./x86_64/config.m4} config.m4
      ln -s $mpnDir/${arch}/${arch}-defs.m4 mpn/${arch}/
      ln -s $mpnDir/asm-defs.m4 mpn/
      cd mpn/
    '';

  compilePopham = operation:
    shellDerivation {
      inherit genodeEnv m4EnvSetup mpnDir arch operation;
      inherit (genodeEnv) cc ccMarch;
      buildInputs = [ nixpkgs.m4 ];
      name = operation+".o";
      script = builtins.toFile "popham.sh"
        ''
          source $genodeEnv/setup
          runHook m4EnvSetup
          MSG_ASSEM $name
          VERBOSE $mpnDir/m4-ccas \
            --m4=m4 $cc $ccMarch -std=gnu99 -fPIC -DPIC \
            -DOPERATION_$operation \
            -c "$mpnDir/$arch/popham.asm" -o $out
        '';
    };
in
linkStaticLibrary rec {
  name = "gmp-mpn-"+gmpSrc.version;
  libs = [ libc ];

  objects = map compilePopham [ "hamdist" "popcount" ];

  externalObjects =
    [ (compileCRepo {
        inherit name libs;
        sources = addPrefix "${gmpSrc}/mpn/" [ "${bits}/*.c" "generic/*.c" ];
        filter =
          [ # this file uses the 'sdiv_qrnnd'
            # symbol which is not defined
            "udiv_w_sdiv.c"
            "pre_divrem_1.c" "popham.c"
            # filter is broken, its filtering out divrem_1.c.
          ];

        ccFlags_add_n = "-DOPERATION_add_n";
        ccFlags_sub_n = "-DOPERATION_sub_n";
        extraFlags = [ "-DHAVE_CONFIG_H" "-D__GMP_WITHIN_GMP" ];
        externalIncludes =
          [ "${gmpSrc}/mpn/generic"
            "${gmpInclude}/${bits}"
            gmpInclude
            gmpSrc.include
            ../../../include/gcc
          ];
      })

      (shellDerivation {
        name = "gmp-asm-objects";
        inherit genodeEnv mpnDir m4EnvSetup;
        inherit (genodeEnv) cc ccMarch;
        buildInputs = [ nixpkgs.m4 ];
        sources =
          [ "${mpnDir}/${arch}/copyd.asm"
            "${mpnDir}/${arch}/copyi.asm"
          ];
        script = ./compile-asm.sh;
      })
  ];

}
