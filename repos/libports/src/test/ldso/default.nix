{ genodeEnv, compileCC
, base, test-ldso_lib_1, test-ldso_lib_2, libc, libm, ld }:

genodeEnv.mkComponent rec {
  name = "test-ldso";
  libs = [ test-ldso_lib_1 test-ldso_lib_2 libc libm ld ];
  objects = compileCC {
    src = ./main.cc;
    localIncludes = [ ./include ];

    systemIncludes = genodeEnv.tool.propagatedIncludes libs;
    # TODO: is this a worthwhile function? ^
  };
}
