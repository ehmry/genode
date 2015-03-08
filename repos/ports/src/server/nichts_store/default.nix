{ linkComponent, compileCC
, libc, libm, stdcxx
, nixmain, nixutil, nixexpr, nixformat, nixstore, server }:

linkComponent rec {
  name = "nichts_store";
  libs = [ server nixmain stdcxx nixutil nixexpr nixformat nixstore libc ];
  objects = compileCC {
    src = ./main.cc;
    inherit libs;
  };
}