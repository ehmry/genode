{ linkComponent, compileCC, filterHeaders
, base, alarm, config }:

linkComponent rec {
  name = "noux";
  libs = [ base alarm config ];
  objects = map
    ( src: compileCC {
        inherit src libs;
        systemIncludes = map
          filterHeaders
          [ ../../noux ../../../include ];
      }
    )
    [ ../main.cc ./dummy_net.cc ];
}
