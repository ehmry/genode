{ genodeEnv, compileCC, base, config }:

genodeEnv.mkComponent {
  name = "bomb";
  libs = [ base config ];
  objects = compileCC { src = ./main.cc; };
}
