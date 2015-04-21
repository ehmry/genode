{ linkStaticLibrary, compileCC }:

linkStaticLibrary {
  name = "blake2s";
  objects = compileCC {
    src = ./blake2s.cc;
  };
}