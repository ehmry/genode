{ genodeEnv, timer }:
genodeEnv.mkComponent {
  name = "timer";
  libs = [ timer ];
  sources = genodeEnv.fromPath ./empty.cc;
}
