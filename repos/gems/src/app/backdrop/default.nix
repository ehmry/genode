{ linkComponent, compileCC
, base, config, libc, libpng, zlib, blit, file }:

linkComponent rec {
  name= "backdrop";
  libs = [ base config libc libpng zlib blit file ];
  objects = compileCC {
    src = ./main.cc;
    inherit libs;
    externalIncludes = [ ../../../../demo/include ];
  };
}
