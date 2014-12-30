{ linkComponent, compileCC, shellDerivation, genodeEnv, nixpkgs, lx_hybrid }:

let
 testlib = shellDerivation {
    name = "lx_testlib";
    buildInputs = [ nixpkgs.gcc ];
    source = ./testlib.cc;
    header = ./testlib.h;
    inherit genodeEnv;
    inherit (genodeEnv) ccMarch;
    script = builtins.toFile "lx_testlib.sh"
      ''
        source $genodeEnv/setup

        cp $source ./testlib.cc
        cp $header ./testlib.h
        mkdir $out
        MSG_BUILD libtestlib.so
        VERBOSE g++ $ccMarch -fPIC -c testlib.cc
        VERBOSE g++ $ccMarch -shared -o $out/libtestlib.so testlib.o
      '';
  };
in
linkComponent {
  name = "test-lx_hybrid_ctors";
  libs = [ lx_hybrid ];

  objects =
    [ (compileCC { src = ./main.cc; })
      "${testlib}/libtestlib.so"
    ];
}
