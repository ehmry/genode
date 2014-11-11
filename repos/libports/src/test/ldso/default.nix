{ genodeEnv, test-ldso_lib_1, test-ldso_lib_2, libc, libm, ld }:

genodeEnv.mkComponent {
  name = "test-ldso";
  libs = [ test-ldso_lib_1 test-ldso_lib_2 libc libm ld ];
  sources = genodeEnv.fromPath ./main.cc;
  localIncludes = [ ./include ];
}
