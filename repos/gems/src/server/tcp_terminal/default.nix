{ linkComponent, compileCC, libc, libc_lwip_nic_dhcp, libc_lock_pipe }:

linkComponent rec {
  name = "tcp_terminal";
  libs = [ libc libc_lwip_nic_dhcp libc_lock_pipe ];
  objects = compileCC {
    src = ./main.cc;
    inherit libs;
  };
}

