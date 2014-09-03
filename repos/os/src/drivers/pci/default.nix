{ build, base, os }:

if build.isx86 then build.driver {
  name = "pci_drv";
  libs = [ base.lib.base os.lib.config ];
  sources = [ ./main.cc ];
  includeDirs =
    [ ../pci ]
     ++ os.includeDirs
     ++ base.includeDirs;
} else null