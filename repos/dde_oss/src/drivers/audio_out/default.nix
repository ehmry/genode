{ spec, linkComponent, compileCC, compileC, compileCRepo
, ossSrc, dde_kit }:

let

  ccFlags = [ "-Ulinux" "-D_KERNEL" "-fno-omit-frame-pointer" ];

  cFlags = ccFlags ++
    [ "-Wno-unused-variable"
      "-Wno-unused-but-set-variable"
      "-Wno-implicit-function-declaration"
      "-Wno-sign-compare"
    ];

  systemIncludes =
    [ "${ossSrc}/kernel/framework/include"
      "${ossSrc}/include"
      ./include
    ];
    
in
if spec.isx86 then linkComponent rec {
  name = "audio_out_drv";
  libs = [ dde_kit ];

  # Native Genode sources
  objects =

    (map
      (src: compileCC { inherit src systemIncludes; extraFlags = ccFlags; })
      [ ./os.cc ./main.cc ./driver.cc ./signal/irq.cc ./quirks.cc ]
    ) ++
    [ (compileC { src = ./dummies.c; extraFlags = cFlags; inherit libs systemIncludes; }) ];

  # Driver Sources
  externalObjects = compileCRepo {
    sourceRoot = "${ossSrc}/kernel/framework";
    sources =
      [ "osscore/*.c"
        "audio/*.c"
        "mixer/*.c" 
        "vmix_core/*.c" 
        "midi/*.c"
        "ac97/*.c"
        "${ossSrc}/kerne/drv/*/*.c"
      ];        

    extraFlags = cFlags;
    inherit systemIncludes libs;
  };
  
} else { }
