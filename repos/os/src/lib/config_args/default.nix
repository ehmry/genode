{ genodeEnv, compileCC, config }:

genodeEnv.mkLibrary {
  name = "config_args";
  libs = [ config ];
  objects = compileCC { src = ./config_args.cc; };
}
