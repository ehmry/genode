{ build, base }:

build.test {
  name = "test-fpu";
  libs = [ base.lib.base ];
  sources = [ ./main.cc ];
  inherit (base) includeDirs;
}