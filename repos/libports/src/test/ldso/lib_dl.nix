{ linkSharedLibrary, compileCC, test-ldso_lib_1 }:

linkSharedLibrary {
  name = "test-ldso_lib_dl";
  libs = [ test-ldso_lib_1 ];
  objects = compileCC {
    src = ./lib_dl.cc;
    localIncludes = [ ./include ];
  };
}
