{ linkStaticLibrary, compileCC }:
linkStaticLibrary {
   name = "init_pd_args";
   objects = compileCC { src = ../../init/pd_args.cc; };
}
