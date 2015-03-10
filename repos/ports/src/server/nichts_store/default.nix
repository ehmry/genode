{ linkComponent, compileCC
, libc, libm, stdcxx
, config, server }:

linkComponent rec {
  name = "nichts_store";
  libs = [ server config stdcxx ];
  objects = compileCC {
    src = ./main.cc;
    inherit libs;
  };
}