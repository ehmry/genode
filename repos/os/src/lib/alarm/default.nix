{ genodeEnv, compileCC }:
genodeEnv.mkLibrary {
  name = "alarm"; objects = compileCC { src = ./alarm.cc; };
}
