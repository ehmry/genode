{ build, base, os }:

if build.isx86 then build.driver {
  name = "ps2_drv";
  libs = [ base.lib.base ];
  sources = [ ./x86/main.cc ];
  includeDirs =
   [ ./x86 ../ps2 ]
   ++ os.includeDirs
   ++ base.includeDirs;
} else 
null