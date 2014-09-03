{ build, base, os, demo }:

build.server {
  name = "liquid_fb";
  libs = [ demo.lib.scout_widgets os.lib.config ];
  sources = [ ./main.cc ./services.cc ];
  includeDirs =
    [ ../liquid_framebuffer
      ../../app/scout
      demo.includeDir
      os.includeDir
    ] ++ base.includeDirs;
}