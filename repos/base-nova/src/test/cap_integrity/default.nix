{ genodeEnv, compileCC, base }:

genodeEnv.mkComponent {
  name = "test-cap_integrity";
  libs = [ base ];
  objects = compileCC { src = ./main.cc; };
}
