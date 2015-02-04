{ linkSharedLibrary, compileCRepo, compileCCRepo
, shellDerivation, genodeEnv, fromDir
, icuSrc, stdcxx, pthread }:

let
  # provide the binary file 'icudt51l.dat' as symbol 'icudt51_dat'
  icuDat = shellDerivation {
    name = "binary_icudt51l.dat.o";
    inherit icuSrc genodeEnv;
    inherit (genodeEnv) as asFlags;
    script = builtins.toFile "icudt51l.dat.sh" ''
      source $genodeEnv/setup
      echo ".global icudt51_dat; .section .rodata; .align 4; icudt51_dat:; .incbin \"$icuSrc/data/in/icudt51l.dat\"" |\
          $as $asFlags -f -o $out -
    '';
  };

  extraFlags =
    [ "-DU_COMMON_IMPLEMENTATION"
      "-DU_I18N_IMPLEMENTATION"
      "-Wno-deprecated-declarations"
    ];

  externalIncludes = [ (icuSrc + "/common") ];

  fromIcu = fromDir icuSrc;
in
linkSharedLibrary rec {
  name = "icu";
  libs = [ stdcxx pthread ];

  # provide the binary file 'icudt51l.dat' as symbol 'icudt51_dat'
  objects = [ icuDat ];

  externalObjects =
    [ ( compileCRepo {
          sources = fromIcu [ "common/*.c" "il8n/*.c" ];
          inherit extraFlags externalIncludes libs;
        }
      )
      ( compileCCRepo {
          sources = fromIcu [ "common/*.cpp" "il8n/*.cpp" ];
          inherit extraFlags externalIncludes libs;
        }
      )
    ];

  propagate.externalIncludes = fromDir icuSrc.include
    [ "icu/common" "icu/il8n" ];

}
