{ genodeEnv, linkSharedLibrary, compileCC, libc }:

let
  compileCC' = src: compileCC {
    inherit src;
    systemIncludes = genodeEnv.tool.propagateIncludes [ libc ];
  };
in
linkSharedLibrary {
  name = "pthread";
  libs = [ libc ];
  objects = map
    compileCC'
    [ ./semaphore.cc ./thread.cc ./thread_create.cc ];
}
