{ genodeEnv, test-ldso_lib_2, ldso-startup }:

genodeEnv.mkLibrary {
  name = "test-ldso_lib_1";
  shared = true;
  libs = [ test-ldso_lib_2 ldso-startup ];
  src = [ ../../src/test/ldso/lib_1.cc ];
  incDir = [ ../../src/test/ldso/include ];
}
