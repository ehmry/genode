{ genodeEnv, compileCC, lx_hybrid }:

build.test {
  name = "test-lx_hybrid_errno";
  libs = [ lx_hybrid ];
  objects = compileCC { src = ./main.cc; };
}
