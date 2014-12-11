{ linkSharedLibrary, compileCC, libc }:

linkSharedLibrary rec {
  name = "pthread";
  libs = [ libc ];
  objects = map
    (src: compileCC { inherit src libs; })
    [ ./semaphore.cc ./thread.cc ./thread_create.cc ];
}
