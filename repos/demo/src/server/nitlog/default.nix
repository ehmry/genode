{ build }:
{ base, blit }:

build.component {
  name = "nitlog";
  libs = [ base blit ];
  sources  = [ ./main.cc ];
  binaries = [ ./mono.tff ];
}