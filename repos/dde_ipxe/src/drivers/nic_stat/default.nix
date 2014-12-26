{ linkComponent, compileCC, dde_ipxe_nic, net-stat }:

linkComponent rec {
  name = "nic_drv_stat";
  libs = [ dde_ipxe_nic net-stat ];
  objects = [( compileCC { src = ./main.cc; inherit libs; } )];
}
