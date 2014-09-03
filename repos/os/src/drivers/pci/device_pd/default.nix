{ build, base, os }:
let
  pciDir = ../../pci;
in
if build.isNova then  build.driver {
  name = "pci_device_pd";
  libs = [ base.lib.base os.lib.config ];
  sources = [ ./main.cc ];
  includeDirs =
    [ "${pciDir}/device_pd" ]
     ++ os.includeDirs
     ++ base.includeDirs;
} else null