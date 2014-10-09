{ genodeEnv, base }:

genodeEnv.mkComponent {
  name = "test-timer";
  libs = [ base ];
  sources = genodeEnv.fromPath ./main.cc;
}
