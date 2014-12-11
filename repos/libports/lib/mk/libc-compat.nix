{ linkStaticLibrary, compileLibc }:

linkStaticLibrary {
  name = "libc-compat";
  externalObjets = compileLibc {
    sources = [ "lib/libc/compat-43/*.c" ];
  };
}
