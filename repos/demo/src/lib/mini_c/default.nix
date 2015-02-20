{ linkStaticLibrary, compileC, compileCC }:

let
  externalIncludes = [ ../../../include/mini_c ];
in
linkStaticLibrary {
  name = "mini_c";
  objects =
    (map (src: compileCC { inherit src externalIncludes; })
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
        inherit externalIncludes;
      })
    ];
  propagate = { inherit externalIncludes; };
}
