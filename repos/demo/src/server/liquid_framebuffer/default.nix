{ build }:

{ config, scout_widgets }:

build.component {
  name = "liquid_fb";
  libs = [ config scout_widgets ];
  sources = [ ./main.cc ./services.cc ];
  includeDirs =
    [ ../liquid_framebuffer
      ../../app/scout
    ];
}