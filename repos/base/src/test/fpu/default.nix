{ genodeEnv, compileCC, base }:

genodeEnv.mkComponent {
  name = "test-fpu";
  libs = [ base ];
  objects = compileCC { src = ./main.cc; };
}
