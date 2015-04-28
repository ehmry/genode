{ linkComponent, compileCC, server, config, libc, libc_lwip_nic_dhcp }:

linkComponent rec {
  name = "9p_fs";
  libs = [ server config libc libc_lwip_nic_dhcp ];
  objects = compileCC { src = ./main.cc; inherit libs; };
  runtime.provides = [ "File_system" ];
}
