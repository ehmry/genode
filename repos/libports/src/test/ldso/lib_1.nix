{ linkSharedLibrary, compileCC, test-ldso_lib_2 }:

linkSharedLibrary {
  name = "test-ldso_lib_1";
  libs = [ test-ldso_lib_2 ];
  objects = compileCC {
    src = ./lib_1.cc;
    includes = [ ./include ];
  };
}
