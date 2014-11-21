{ genodeEnv, compileCC, base }:

genodeEnv.mkComponent {
  name = "test-signal";
  libs = [ base ];
  objects = compileCC { src = ./main.cc; };
}
