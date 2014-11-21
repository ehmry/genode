{ genodeEnv, compileCC, libc, libm, ld }:

genodeEnv.mkComponent {
  name = "test-libc";
  objects = compileCC { src = ./main.cc; };
  libs = [ libm libc ld ];
}
