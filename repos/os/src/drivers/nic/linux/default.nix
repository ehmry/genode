{ spec, linkComponent, compileCC, lx_hybrid, config }:

if !spec.isLinux then {} else
linkComponent rec {
  name = "nic_drv";
  libs = [ lx_hybrid config ];
  objects = compileCC {
    inherit libs;
    src = ./main.cc;
  };
}
