{ genodeEnv, base, config }:

let
  pciDir = ../../pci;
in
if genodeEnv.isNova then  genodeEnv.mkComponent {
  name = "pci_device_pd";
  libs = [ base config ];
  sources = genodeEnv.fromPath ./main.cc;
} else null
