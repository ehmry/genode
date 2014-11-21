{ build, compileCC, base, x86emu, blit, config }:

let
  compileCC = src: compileCC {
    inherit src;
    includeDirs = [ ./include ../../../include/x86emu ]
  };
in
build.driver {
  name = "fb_drv";
  libs = [ x86emu blit base config ];
  objects = map compileCC'
    [ ./main.cc ./framebuffer.cc ./ifx86emu.cc ./hw_emul.cc ];
}
