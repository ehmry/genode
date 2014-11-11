{ genodeEnv, libc, libm, ld }:

genodeEnv.mkComponent {
  name = "test-libc";
  sources = genodeEnv.fromPath ./main.cc;
  libs = [ libm libc ld ];
}
