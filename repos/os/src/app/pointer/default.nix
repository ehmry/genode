{ genodeEnv, compileCC, base }:

genodeEnv.mkComponent {
  name = "pointer";
  libs = [ base ];
  objects = compileCC { src = ./main.cc; };
}
