{ genodeEnv, compileCC, base, init_pd_args, config }:

genodeEnv.mkComponent {
  name = "init";
  libs = [ base init_pd_args config ];
  objects = compileCC { src = ./main.cc; };
}
