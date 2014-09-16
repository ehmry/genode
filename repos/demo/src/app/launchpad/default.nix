{ build }:
{ launchpad, scout_widgets, config }:

build.component {
  name = "launchpad";
  libs = [ launchpad scout_widgets config ];
  sources = [ ./launchpad_window.cc ./launcher.cc ./main.cc ];
  includeDirs = [ ../launchpad ../scout ];
}