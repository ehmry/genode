{ genodeEnv, ldso-startup }:

genodeEnv.mkLibrary {
  name = "test-ldso_lib_2";
  shared = true;
  libs = [ ldso-startup ];
  src = [ ../../src/test/ldso/lib_2.cc ];
  incDir = [ ../../src/test/ldso/include ];
}
