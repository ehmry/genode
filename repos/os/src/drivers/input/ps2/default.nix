{ build }:
{ base }:

if build.isx86 then build.component {
  name = "ps2_drv";
  libs = [ base ];
  sources = [ ./x86/main.cc ];
  includeDirs = [ ./x86 ../ps2 ];
} else null