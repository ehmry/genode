{ linkComponent, compileCC, lx_hybrid }:

linkComponent rec {
  name = "test-lx_hybrid_exception";
  libs = [ lx_hybrid ];
  objects = compileCC { src = ./main.cc; inherit libs; };
}
