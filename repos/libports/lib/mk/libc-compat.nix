{ linkStaticLibrary, compileSubLibc }:

linkStaticLibrary {
  name = "libc-compat";
  externalObjets = compileSubLibc {
    sources = [ "lib/libc/compat-43/*.c" ];
  };
}
