{ build }:
{ base }:

build.component {
  name = "test-thread";
  sources = [ ./main.cc ];
  libs = [ base ];
}