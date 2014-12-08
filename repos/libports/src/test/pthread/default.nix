{ genodeEnv, linkComponent, compileCC, libc, pthread }:

linkComponent rec {
  name = "test-pthread";
  libs = [ libc pthread ];
  objects = compileCC {
    src = ./main.cc;
    systemIncludes = genodeEnv.tool.propagateIncludes libs;
  };
}
