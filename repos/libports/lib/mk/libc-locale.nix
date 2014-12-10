{ linkStaticLibrary, compileSubLibc }:

linkStaticLibrary {
  name = "libc-locale";
  externalObjects = compileSubLibc {
    sources = [ "lib/libc/locale/*.c" ];
  };
}
