{ build }:

{ base }:

if build.isx86 then build.component {
  name = "acpi_drv";
  libs = [ base ];
  sources = [ ./main.cc ./acpi.cc ];
  includeDirs = [ ../acpi ];
} else null