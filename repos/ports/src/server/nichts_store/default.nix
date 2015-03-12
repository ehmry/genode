{ linkComponent, compileCC
, base, server, config }:

linkComponent rec {
  name = "nichts_store";
  libs = [ base server config ];
  objects = map
    (src: compileCC { inherit src libs; })
    [ ./main.cc ./noux.cc ];
}