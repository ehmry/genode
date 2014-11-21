{ genodeEnv, compileCC, base }:

genodeEnv.mkComponent {
  name = "test-input";
  libs = [ base ];
  objects = compileCC { src = ./test.cc; };
}
