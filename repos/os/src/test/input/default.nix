{ build, base, os }:

build.test {
  name = "test-input";
  libs = [ base.lib.base ];
  sources = [ ./test.cc ];
  includeDirs = os.includeDirs ++ base.includeDirs;
}