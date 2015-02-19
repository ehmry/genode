{ linkComponent, compileCC, filterHeaders
, base, alarm, config, libc_noux }:

let libs = [ base alarm config ]; in
linkComponent {
  name = "noux";
  libs = libs;
  objects = map
    ( src: compileCC {
        inherit src libs;
        includes = [ ../../noux ../../../include ];
    })
    [ ../main.cc ./dummy_net.cc ];

  runtime.libs = libs ++ [ libc_noux ];
}
