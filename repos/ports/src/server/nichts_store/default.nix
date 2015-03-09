{ linkComponent, compileCC
, libc, libm, stdcxx
, nixmain, server }:

linkComponent rec {
  name = "nichts_store";
  libs = [ server nixmain stdcxx ];
  objects = compileCC {
    src = ./main.cc;
    inherit libs;
  };
}