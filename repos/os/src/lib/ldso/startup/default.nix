{ genodeEnv, compileCC }:

genodeEnv.mkLibrary {
  name = "ldso-startup";
  objects = compileCC { src = ./startup.cc; };
}
