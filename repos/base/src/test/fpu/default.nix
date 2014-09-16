{ build }:
{ base }:

build.component {
  name = "test-fpu";
  libs = [ base ];
  sources = [ ./main.cc ];
}