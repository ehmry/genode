{ linkStaticLibrary, compileCC, base, config }:

linkStaticLibrary {
  name = "launchpad";
  libs = [ base config ];
  objects = compileCC { src = ./launchpad.cc; };
}
