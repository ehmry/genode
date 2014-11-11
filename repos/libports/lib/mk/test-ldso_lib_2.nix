{ genodeEnv, ldso-startup }:

genodeEnv.mkLibrary {
  name = "test-ldso_lib_2";
  shared = true;
  libs = [ ldso-startup ];
  sources = genodeEnv.fromPath ../../src/test/ldso/lib_2.cc;
  localIncludes = [ ../../src/test/ldso/include ];
}
