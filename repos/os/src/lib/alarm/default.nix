{ genodeEnv, timer }:
genodeEnv.mkLibrary { 
  name = "alarm"; sources = genodeEnv.fromPaths [ ./alarm.cc ];
}
