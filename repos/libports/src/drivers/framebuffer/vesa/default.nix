{ build, base, os, libports }:

build.driver {
  name = "fb_drv";
  libs =
    [ libports.lib.x86emu os.lib.blit 
      base.lib.base os.lib.config
    ];
  sources = 
    [ ./main.cc ./framebuffer.cc ./ifx86emu.cc ./hw_emul.cc ];
  includeDirs =
    [ ./include "${libports.port.x86emu}" ]
    ++ os.includeDirs
    ++ base.includeDirs;
}