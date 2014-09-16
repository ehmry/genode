{ build }:
{ base, config }:

if build.isx86 then build.component {
  name = "pci_drv";
  libs = [ base config ];
  sources = [ ./main.cc ];
  includeDirs = [ ../pci ];
} else null