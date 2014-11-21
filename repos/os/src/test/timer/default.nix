{ genodeEnv, compileCC, base }:

genodeEnv.mkComponent {
  name = "test-timer";
  libs = [ base ];
  objects = compileCC { src = ./main.cc; };
}
