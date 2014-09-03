{ build, base, os, demo }:
build.library {
  name = "scout_gfx";
  sources = [ ./sky_texture_painter.cc ];
  includeDirs =
    [ demo.includeDir os.includeDir ] ++ base.includeDirs;
}