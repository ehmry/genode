{ linkComponent, compileCC, libc }:

linkComponent rec {
  name = "test-libc_vfs";
  libs = [ libc ];
  objects = [(compileCC {
    # we re-use the libc_ffat test
    src = ../libc_ffat/main.cc;
    inherit libs;
  })];
}
