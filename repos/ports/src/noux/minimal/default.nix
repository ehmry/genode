{ linkComponent, compileCC, filterHeaders
, base, alarm, config }:

linkComponent {
  name = "noux";
  libs = [ base alarm config ];
  objects = map
    ( src: compileCC {
        inherit src;
        systemIncludes = map
          filterHeaders
          [ ../../noux ../../../include ];
      }
    )
    [ ../main.cc ./dummy_net.cc ];
}
