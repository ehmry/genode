{ genodeEnv, timer }:
genodeEnv.mkLibrary { 
  name = "alarm"; sources = genodeEnv.fromPath ./alarm.cc;
}
