{ linkStaticLibrary, compileCC }:

linkStaticLibrary {
  name = "dde_ipxe_support";
  objects = [( compileCC { src = ./dde_support.cc; } )];
}
