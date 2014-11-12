{ genodeEnv, base }:

genodeEnv.mkComponent {
  name = "test-fpu"; libs = [ base ];
  sources = genodeEnv.fromPath ./main.cc;
}
