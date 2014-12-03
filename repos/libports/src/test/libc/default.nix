{ genodeEnv, linkComponent, compileCC, libc, libm }:

linkComponent rec {
  name = "test-libc";
  libs = [ libm libc ];

  objects = compileCC {
    src = ./main.cc;
    systemIncludes = genodeEnv.tool.propagateIncludes libs;
  };
}
