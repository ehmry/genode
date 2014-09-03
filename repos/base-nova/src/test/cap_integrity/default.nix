{ build, base }:

build.test {
  name = "test-cap_integrity";
  libs = [ base.lib.base ];
  sources = [ ./main.cc ];
  inherit (base) includeDirs;
}