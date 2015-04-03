{ linkComponent, compileCC, dde_ipxe_nic }:

linkComponent rec {
  name = "nic_drv";
  libs = [ dde_ipxe_nic ];
  objects = compileCC {
    src = ../main.cc;
    inherit libs;
    includes = [ ../../../../include ../../../../include/dde_ipxe ];
  };

}
