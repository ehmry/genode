{ linkStaticLibrary, compileCC }:

linkStaticLibrary {
  name = "server";
  objects = compileCC { src = ./server.cc; };
}
