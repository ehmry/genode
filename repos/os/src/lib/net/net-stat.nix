{ linkStaticLibrary, compileCC }:

linkStaticLibrary {
  name = "net-stat";
  objects = [( compileCC { src = ./stat.cc; } )];
}
