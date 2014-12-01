{ genodeEnv, compileCC, compileS, syscall }:

genodeEnv.mkLibrary {
  name = "ldso-startup";
  objects = compileCC { src = ./startup.cc; };
}
