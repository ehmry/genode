{ linkStaticLibrary, compileCC, compileS, syscall }:

linkStaticLibrary {
  name = "ldso-startup";
  objects = compileCC { src = ./startup.cc; };
}
