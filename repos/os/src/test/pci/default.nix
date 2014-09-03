{ build, base, os }:

if build.isx86 then build.test {
  name = "test-pci";
  libs = [ base.lib.base ];
  sources = [ ./test.cc ];
  includeDirs = os.includeDirs ++ base.includeDirs;
} else null