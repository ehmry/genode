{ linkStaticLibrary, compileCC, libc }:

linkStaticLibrary rec {
  name = "file";
  libs = [ libc ];
  objects = compileCC {
    src = ./file.cc;
    inherit libs;
  };
}