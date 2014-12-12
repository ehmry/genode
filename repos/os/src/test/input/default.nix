{ genodeEnv, compileCC, base }:

linkComponent {
  name = "test-input";
  libs = [ base ];
  objects = compileCC { src = ./test.cc; };
}
