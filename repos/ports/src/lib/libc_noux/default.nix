{ linkSharedLibrary, compileCC, libc, env }:

linkSharedLibrary {
  name = "libc_noux";
  libs = [ libc ];
  objects = [(compileCC {
    src = ./plugin.cc;
    libs = [ libc env ];
    systemIncludes = [ ../../../../libports/src/lib/libc ];
  })];
}
