{ linkComponent, compileCC
, base, server, config, init_pd_args}:

linkComponent rec {
  name = "nichts_store";
  libs = [ base server config init_pd_args ];
  objects = map
    (src: compileCC { inherit src libs; })
    [ ./main.cc ];
}