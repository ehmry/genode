{ linkSharedLibrary, compileCC, libc }:

linkSharedLibrary rec {
  name = "libc_terminal";
  libs = [ libc ];
  objects = compileCC {
    src = ./plugin.cc; inherit libs;
  };
}