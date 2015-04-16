{ linkComponent, compileCC, libc, libm, stdcxx, nixexpr, nixformat, nixstore, nixutil, nixmain }:

linkComponent rec {
  name = "nix-instantiate";
  libs = [ libc libm stdcxx nixexpr nixformat nixstore nixutil nixmain ];
  objects = compileCC {
    src = ./main.cc;
    inherit libs;
  };
}
