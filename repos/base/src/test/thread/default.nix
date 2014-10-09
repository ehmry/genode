{ genodeEnv, base }:

genodeEnv.mkComponent {
  name = "test-thread";
  sources = genodeEnv.fromPath ./main.cc;
  libs = [ base ];
}
