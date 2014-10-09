{ genodeEnv, base, init_pd_args, config }:

genodeEnv.mkComponent {
  name = "init";
  libs = [ base init_pd_args config ];
  sources = genodeEnv.fromPath ./main.cc;
}
