{ linkComponent, compileCC
, libc, libm, stdcxx
, nixmain, nixutil, nixexpr, nixformat, nixstore }:

linkComponent rec {
  name = "nichts_instantiate";
  libs = [ libc libm nixmain stdcxx nixutil nixexpr nixformat nixstore ];
  objects = compileCC {
    src = ./main.cc;
    inherit libs;
    extraFlags = map (i: "-I"+i) libc.propagate.externalIncludes;
  };
}