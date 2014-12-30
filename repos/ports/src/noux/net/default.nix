{ linkComponent, compileCC, filterHeaders
, base, alarm, libc, libc_lwip_nic_dhcp, config }:

linkComponent rec {
  name = "noux";
  libs = [ alarm libc libc_lwip_nic_dhcp config ];
  objects = map
    ( src: compileCC {
        inherit src;
        libs = libs ++ [ base ];
        systemIncludes = map
          filterHeaders
          [ ../net ../../noux ../../../include ];
      }
    )
    [ ../main.cc ./net.cc ];
}
