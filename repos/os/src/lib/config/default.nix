{ genodeEnv, compileCC }:

genodeEnv.mkLibrary {
  name = "config"; objects = compileCC { src = ./config.cc; };
}
