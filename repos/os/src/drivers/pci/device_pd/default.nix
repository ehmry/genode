{ genodeEnv, compileCC, base, config }:

if genodeEnv.isNova then genodeEnv.mkComponent {
  name = "pci_device_pd";
  libs = [ base config ];
  objects = compileCC { src = ./main.cc; };
} else null
