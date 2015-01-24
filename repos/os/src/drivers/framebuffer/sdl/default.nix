{ linkComponent, compileCC, filterHeaders, nixpkgs, lx_hybrid }:

linkComponent rec {
  name = "fb_sdl";
  libs = [ lx_hybrid ];
  lxLibs = [ nixpkgs.SDL ];
  objects = map
    (src: compileCC {
      inherit src libs ;
      systemIncludes = [ (filterHeaders ../sdl) "${nixpkgs.SDL}/include"  ];
    })
    [ ./fb_sdl.cc ./input.cc ];
}
