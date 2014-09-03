{ build, base, os }:

if !build.isx86 then null else build.driver {
  name = "acpi_drv";
  libs = [ base.lib.base ];
  sources = [ ./main.cc ./acpi.cc ];
  includeDirs =
    [ ../acpi ]
     ++ os.includeDirs
     ++ base.includeDirs;
}