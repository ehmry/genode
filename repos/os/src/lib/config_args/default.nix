{ linkStaticLibrary, compileCC, config }:

linkStaticLibrary {
  name = "config_args";
  libs = [ config ];
  objects = compileCC { src = ./config_args.cc; };
}
