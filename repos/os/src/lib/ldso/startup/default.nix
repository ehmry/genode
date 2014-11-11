{ genodeEnv }:

genodeEnv.mkLibrary {
  name = "ldso-startup"; sources = genodeEnv.fromPath ./startup.cc;
}
