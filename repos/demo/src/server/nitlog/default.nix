{ build, base, os, demo }:

build.server {
  name = "nitlog";
  libs = [ base.lib.base os.lib.blit ];
  sources  = [ ./main.cc ];
  binaries = [ ./mono.tff ];
  includeDirs = os.includeDirs ++ base.includeDirs;
}