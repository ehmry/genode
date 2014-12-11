{ linkComponent, compileCC, libc, pthread }:

linkComponent rec {
  name = "test-pthread";
  libs = [ libc pthread ];
  objects = compileCC {
    inherit libs;
    src = ./main.cc;
  };
}
