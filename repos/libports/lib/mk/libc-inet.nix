{ linkStaticLibrary, compileSubLibc }:
linkStaticLibrary {
  name = "libc-inet";
  externalObjects = compileSubLibc {
    sources = [ "lib/libc/inet/*.c" ];
  };
}
