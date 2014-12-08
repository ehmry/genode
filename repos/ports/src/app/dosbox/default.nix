{ genodeEnv, linkComponent, compileCCRepo
, dosboxSrc
, libc, libm, libpng, sdl, sdl_net, stdcxx, zlib, libc_lwip_nic_dhcp, config_args }:

let
  srcDirs = map
    (d: "src/${d}")
    [ "cpu" "debug" "dos" "fpu" "gui"
      "hardware" "hardware/serialport" "ints" "misc" "shell"
    ];

  ccWarn =
    [ "-Wall"
      "-Wno-unused-variable" "-Wno-unused-function" "-Wno-switch" "-Wno-unused-value"
      "-Wno-unused-but-set-variable" "-Wno-format" "-Wno-maybe-uninitialized"
      "-Wno-sign-compare" "-Wno-narrowing" "-Wno-missing-braces" "-Wno-array-bounds"
      "-Wno-parentheses"
    ];
in
linkComponent rec {
  name = "dosbox";
  
  libs =
    [ libc libm libpng sdl sdl_net stdcxx
      zlib libc_lwip_nic_dhcp config_args
    ];

  excternalObjects = compileCCRepo {
    inherit name;
    sourceRoot = dosboxSrc;
    sources = [ "src/dosbox.cpp" ] ++ srcDirs;

    extraFlags =
      [ "-DHAVE_CONFIG_H" "-D_GNU_SOURCE=1" "-D_REENTRANT" ] ++
      ( if genodeEnv.isx86_32 then [ "-DC_TARGETCPU=X86" ] else
        if genodeEnv.isx86_64 then [ "-DC_TARGETCPU=X86_64" ] else
        [ ]
      ) ++
      ccWarn;

    systemIncludes =
      [ ../dosbox ] ++
      ( if genodeEnv.isx86_32 then [ ./x86_32 ] else
        if genodeEnv.isx86_64 then [ ./x86_64 ] else
        [ ]
      );
      
    # Missing some includes.

  };
    
}
