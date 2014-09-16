{ build }:
{ base, config }:

let
  pciDir = ../../pci;
in
if build.isNova then  build.component {
  name = "pci_device_pd";
  libs = [ base config ];
  sources = [ ./main.cc ];
  includeDirs = [ ../../pci/device_pd ];
} else null