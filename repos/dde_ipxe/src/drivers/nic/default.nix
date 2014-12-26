{ linkComponent, compileCC, dde_ipxe_nic }:

linkComponent {
  name = "nic_drv";
  libs = [ dde_ipxe_nic ];
  objects = [( compileCC {
    src = ./main.cc;
    systemIncludes = [ ../../../include ];
  } )];

}
