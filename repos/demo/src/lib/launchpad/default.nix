{ tool }:
{ base }:

tool.buildLibrary {
  name = "launchpad";
  libs = [ base ];
  sources = [ ./launchpad.cc ];
}