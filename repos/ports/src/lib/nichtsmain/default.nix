{ linkStaticLibrary, compileCC, config, stdcxx, nixutil, nixSrc}:

linkStaticLibrary rec {
  name = "nixmain";
  libs = [ config stdcxx nixutil ];
  objects = map
    (src: compileCC { inherit src libs; })
    [ ./config.cc ./init.cc ];
  propagate.externalIncludes = [ ../../../include ];
}