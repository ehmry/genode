{ linkComponent, compileCC
, config, libc, libpng, zlib, blit, file }:

linkComponent rec {
  name= "backdrop";
  libs = [ config libc libpng zlib blit file ];
  objects = compileCC {
    src = ./main.cc;
    inherit libs;
    externalIncludes = [ ../../../../demo/include ];
  };
}
