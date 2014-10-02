{ tool }: with tool;
{ test-ldso_lib_1, test-ldso_lib_2, libc, libm, ld }:

buildComponent {
  name = "test-ldso";
  libs = [ test-ldso_lib_1 test-ldso_lib_2 libc libm ld ];
  sources = [ ./main.cc ];
  includeDirs = [ ./include ];
}