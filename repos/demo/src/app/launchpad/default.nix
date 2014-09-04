{ build, base, os, demo }:

build.component {
  name = "launchpad";
  libs = [ demo.lib.launchpad demo.lib.scout_widgets os.lib.config ];
  sources = [ ./launchpad_window.cc ./launcher.cc ./main.cc ];
  includeDirs =
    [ ../launchpad ../scout
      demo.includeDir ] 
    ++ os.includeDirs ++ base.includeDirs;
}