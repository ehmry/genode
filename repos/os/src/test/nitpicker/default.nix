{ genodeEnv, compileCC, base }:

genodeEnv.mkComponent {
  name = "testnit";
  libs = [ base ];
  objects = compileCC { src = ./test.cc; };
}
