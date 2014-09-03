{ build, base, os }:

build.test {
  name = "testnit";
  libs = [ base.lib.base ];
  sources = [ ./test.cc ];
  includeDirs = os.includeDirs ++ base.includeDirs;
}