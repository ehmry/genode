{ genodeEnv, linkComponent, compileCC, libc, libm }:

linkComponent rec {
  name = "test-libc";
  libs = [ libm libc ];

  objects = compileCC {
    inherit libs;
    src = ./main.cc;
  };
}
