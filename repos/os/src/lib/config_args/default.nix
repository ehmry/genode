{ genodeEnv, config }:

genodeEnv.mkLibrary { 
  name = "config_args";
  libs = [ config ];
  sources = genodeEnv.fromPath ./config_args.cc;
}
