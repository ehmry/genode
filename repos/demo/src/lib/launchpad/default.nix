{ genodeEnv, compileCC, base }:

genodeEnv.mkLibrary {
  name = "launchpad";
  libs = [ base ];
  objects = compileCC { src = ./launchpad.cc; };
}
