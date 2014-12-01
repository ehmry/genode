{ genodeEnv, compileCC, test-ldso_lib_1, ldso-startup }:

genodeEnv.mkLibrary {
  name = "test-ldso_lib_dl";
  shared = true;
  libs = [ test-ldso_lib_1 ldso-startup ];
  objects = compileCC {
    src = ./lib_dl.cc;
    localIncludes = [ ./include ];
  };
}
