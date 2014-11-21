{ genodeEnv, compileC, compileCC }:

let
  compileCC' = src: compileCC {
    inherit src;
    systemIncludes = [ ../../../include/mini_c ];
  };
in
genodeEnv.mkLibrary {
  name = "mini_c";
  objects =
    (map compileCC'
      [ ./abort.cc       ./atol.cc
        ./malloc_free.cc ./memcmp.cc
        ./memset.cc
        ./printf.cc      ./snprintf.cc
        ./strlen.cc      ./strtod.cc
        ./strtol.cc      ./vsnprintf.cc
      ]
    ) ++
    [ (compileC {
        src = ./mini_c.c;
        systemIncludes = [ ../../../include/mini_c ];
      })
    ];
}
