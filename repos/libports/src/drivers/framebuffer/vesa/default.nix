{ linkComponent, compileCC, base, x86emu, blit, config }:

let
  systemIncludes = [ ./include x86emu ];
in
linkComponent rec {
  name = "fb_drv";
  libs = [ x86emu blit base config ];
  objects = map (src: compileCC { inherit src libs systemIncludes; })
    [ ./main.cc ./framebuffer.cc ./ifx86emu.cc ./hw_emul.cc ];
}
