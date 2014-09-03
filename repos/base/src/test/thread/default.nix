{ build, base }:

build.test {
  name = "test-thread";
  sources = [ ./main.cc ];
  libs = [ base.lib.base ];
  includeDirs = base.includeDirs;
}