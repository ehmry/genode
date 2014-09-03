{ build, base, os, demo }:
build.library {
  name = "launchpad";
  libs = [ base.lib.base ];
  sources = [ ./launchpad.cc ];
  includeDirs = [ demo.includeDir ] ++ os.includeDirs ++ base.includeDirs;
}