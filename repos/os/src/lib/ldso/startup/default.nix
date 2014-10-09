{ genodeEnv }:

genodeEnv.mkLibrary {
  name = "ldso-startup"; src = [ ./startup.cc ];
}