{ genodeEnv, compileCC }:

genodeEnv.mkLibrary {
  name = "server";
  objects = compileCC { src = ./server.cc; };
}
