{ tool }: with tool;
{ }:
{ test-ldso_lib_2, ldso-startup }:

buildLibraryPlain {
  name = "test-ldso_lib_1";
  shared = true;
  libs = [ test-ldso_lib_2 ldso-startup ];
  sources = [ ../../src/test/ldso/lib_1.cc ];
  includeDirs = [ ../../src/test/ldso/include ];
}