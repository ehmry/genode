{ genodeEnv, compileCC, base, config }:

genodeEnv.mkLibrary {
  name = "launchpad";
  libs = [ base config ];
  objects = compileCC { src = ./launchpad.cc; };
}
