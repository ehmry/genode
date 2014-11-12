{ genodeEnv }:

genodeEnv.mkLibrary { 
  name = "server"; sources = genodeEnv.fromPath ./server.cc;
}
