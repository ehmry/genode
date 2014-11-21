{ genodeEnv, compileCC, timer }:

genodeEnv.mkComponent {
  name = "timer";
  libs = [ timer ];
  objects = compileCC { src = ./empty.cc; };
}
