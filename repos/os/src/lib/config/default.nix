{ linkStaticLibrary, compileCC }:

linkStaticLibrary {
  name = "config"; objects = compileCC { src = ./config.cc; };
}
