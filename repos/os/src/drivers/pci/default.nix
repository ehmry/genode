{ genodeEnv, base, config }:

if genodeEnv.isx86 then genodeEnv.mkComponent {
  name = "pci_drv";
  libs = [ base config ];
  sources = genodeEnv.fromPath ./main.cc;
  #includeDirs = [ ../pci ];
} else null
