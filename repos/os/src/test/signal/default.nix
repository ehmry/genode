{ genodeEnv, base }:

genodeEnv.mkComponent {
  name = "test-signal";
  libs = [ base ];
  sources = genodeEnv.fromPath ./main.cc;
}
