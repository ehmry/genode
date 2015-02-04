{ linkSharedLibrary, compileCC, libc, env }:

linkSharedLibrary {
  name = "libc_noux";
  libs = [ libc ];
  objects = [(compileCC {
    src = ./plugin.cc;
    libs = [ libc env ];
    includes = [ ../../../../libports/src/lib/libc ];
  })];
}
