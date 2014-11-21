{ genodeEnv, compileCC, base, config }:

if genodeEnv.isx86 then genodeEnv.mkComponent {
  name = "pci_drv";
  libs = [ base config ];
  objects = compileCC { src = ./main.cc; };
} else null
