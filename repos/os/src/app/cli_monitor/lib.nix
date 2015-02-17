{ linkStaticLibrary, compileCC }:

linkStaticLibrary {
  name = "cli_monitor";
  objects = compileCC {
    src = ./no_extension.cc;
    includes = [ ../cli_monitor ];
  };
}