{ linkStaticLibrary, compileCC
, lwip, libc
, libc-resolv, libc-isc, libc-nameser, libc-net, libc-rpc }:

linkStaticLibrary rec {
  name = "libc_lwip";
  libs =
    [ lwip libc
      libc-resolv libc-isc libc-nameser libc-net libc-rpc
    ];
  objects = map
    (src: compileCC { inherit src libs; })
    [ ./init.cc ./plugin.cc ];
}
