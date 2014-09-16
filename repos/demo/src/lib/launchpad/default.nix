{ build }:
{ base }:

build.library {
  name = "launchpad";
  libs = [ base ];
  sources = [ ./launchpad.cc ];
}