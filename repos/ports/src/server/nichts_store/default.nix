{ linkComponent, compileCC
, libc, stdcxx
, config, server }:

linkComponent rec {
  name = "nichts_store";
  libs = [ server config stdcxx libc];
  objects = map
    (src: compileCC { inherit src libs; })
    [ ./main.cc ./noux.cc ];
}