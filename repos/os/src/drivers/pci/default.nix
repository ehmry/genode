{ genodeEnv, linkComponent, compileCC, base, config }:

if genodeEnv.isx86 then linkComponent {
  name = "pci_drv";
  libs = [ base config ];
  objects = compileCC { src = ./main.cc; };
} else null
