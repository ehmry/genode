{ linkComponent, compileCC, lx_hybrid }:

linkComponent rec {
  name = "test-lx_hybrid_errno";
  libs = [ lx_hybrid ];
  objects = compileCC { src = ./main.cc; inherit libs; };
}
