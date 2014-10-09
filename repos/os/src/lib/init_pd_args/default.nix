{ genodeEnv }:
genodeEnv.mkLibrary {
   name = "init_pd_args";
   sources = genodeEnv.fromPath ../../init/pd_args.cc;
}
