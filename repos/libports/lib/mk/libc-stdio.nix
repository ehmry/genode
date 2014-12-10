{ linkStaticLibrary, compileSubLibc }:

linkStaticLibrary {
  name = "libc-stdio";
  externalObjects = compileSubLibc {
     sources = [ "lib/libc/stdio/*.c" ];
  };
}
