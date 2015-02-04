{ genodeEnv, linkSharedLibrary, compileCC }:

linkSharedLibrary {
  name = "test-ldso_lib_2";
  objects = compileCC {
    src = ./lib_2.cc;
    includes = [ ./include ];
  };
}
