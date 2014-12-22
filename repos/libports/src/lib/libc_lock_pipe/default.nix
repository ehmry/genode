{ linkSharedLibrary, compileCC, libc }:

linkSharedLibrary rec {
  name = "libc_lock_pipe";
  libs = [ libc ];
  objects = [(compileCC { src = ./plugin.cc; inherit libs;})];
}
