{ linkComponent, compileCC
, base, test-ldso_lib_1, test-ldso_lib_2, libc, libm }:

linkComponent rec {
  name = "test-ldso";
  libs = [ test-ldso_lib_1 test-ldso_lib_2 libc libm ];

  objects = compileCC {
    inherit libs;
    src = ./main.cc;
    localIncludes = [ ./include ];
  };
}
