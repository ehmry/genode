{ tool }: with tool;
{ }:
{ ldso-startup }:

buildLibraryPlain {
  name = "test-ldso_lib_2";
  shared = true;
  libs = [ ldso-startup ];
  sources = [ ../../src/test/ldso/lib_2.cc ];
  includeDirs = [ ../../src/test/ldso/include ];
}