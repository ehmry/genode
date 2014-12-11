{ linkStaticLibrary, compileCC, lwip, libc, libc_lwip }:

linkStaticLibrary rec {
  name = "libc_lwip_nic_dhcp";
  libs = [ lwip libc libc_lwip ];
  objects = map
    (src: compileCC { inherit src libs; })
    [ ./init.cc ./plugin.cc ];
}
