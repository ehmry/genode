{ linkStaticLibrary, compileCC }:

linkStaticLibrary {
  name = "alarm"; objects = compileCC { src = ./alarm.cc; };
}
