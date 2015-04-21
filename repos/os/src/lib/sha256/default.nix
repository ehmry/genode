{ linkStaticLibrary, compileCC }:

linkStaticLibrary {
  name = "sha256";
  objects = compileCC {
    src = ./sha256.cc;
  };
}