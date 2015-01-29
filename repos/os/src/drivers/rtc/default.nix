{ spec, linkComponent, compileCC, base, server, lx_hybrid }:

let
  platform =
    if spec.isLinux then rec
      { libs = [ lx_hybrid ];
        sources = [ ./x86/linux.cc ];
        compileArgs = { inherit libs; };
      }
    else { sources = [ ./x86/rtc.cc ]; };
in
if spec.isx86 then linkComponent {
  name ="rtc_drv";
  libs = [ base server ] ++ platform.libs or [];
  objects = map
    (src: compileCC ({ inherit src; } // platform.compileArgs or {}))
    ([ ./x86/main.cc ] ++ platform.sources);
} else null
