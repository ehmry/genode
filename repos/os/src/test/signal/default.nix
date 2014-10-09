{ genodeEnv, base }:

genodeEnv.mkComponent {
  name = "test-signal";
  libs = [ base ];
  src = genodeEnv.fromPath ./main.cc;
}
