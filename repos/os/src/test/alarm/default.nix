{ genodeEnv, compileCC, base, alarm }:

genodeEnv.mkComponent {
  name = "test-alarm";
  objects = compileCC { src = ./main.cc; };
  libs = [ base alarm ];
}
