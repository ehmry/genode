{ genodeEnv, linkComponent, compileCCRepo, addPrefix, dosboxSrc
, libc, libm, libpng, sdl, sdl_net, stdcxx, zlib, libc_lwip_nic_dhcp
, config_args }:

let
  subCompile = { src, libs ? [] }:
  compileCCRepo{ 
    libs = libs ++ [ libc stdcxx sdl ];
    sourceRoot = dosboxSrc;
    filter = [ "opl.cpp" ];
    sources = dosboxSrc+"/src/"+src;

    headers = [ "${dosboxSrc}/include/dosbox.h" ];

    extraFlags =
      [ "-DHAVE_CONFIG_H" "-D_GNU_SOURCE=1" "-D_REENTRANT" ] ++
      ( if genodeEnv.isx86_32 then [ "-DC_TARGETCPU=X86" ] else
        if genodeEnv.isx86_64 then [ "-DC_TARGETCPU=X86_64" ] else
        [ ]
      ) ++
      [ "-Wall"
        "-Wno-unused-variable" "-Wno-unused-function" "-Wno-switch"
        "-Wno-unused-value" "-Wno-unused-but-set-variable"
        "-Wno-format" "-Wno-maybe-uninitialized" "-Wno-sign-compare"
        "-Wno-narrowing" "-Wno-missing-braces" "-Wno-array-bounds"
        "-Wno-parentheses"
      ];

    includes =
      [ ../dosbox ] ++
      ( if genodeEnv.isx86_32 then [ ./x86_32 ] else
        if genodeEnv.isx86_64 then [ ./x86_64 ] else
        [ ]
      );
    externalIncludes = [ "${dosboxSrc}/include" ];
  };

in
linkComponent rec {
  name = "dosbox";

  libs =
    [ libc libm libpng sdl sdl_net stdcxx
      zlib libc_lwip_nic_dhcp config_args
    ];

  externalObjects = map
    subCompile
    [ { src = "dosbox.cpp"; }
      { src = "cpu/*.cpp"; }
      { src = "debug/*.cpp"; }
      { src = "dos/*.cpp"; }
      { src = "fpu/*.cpp"; }
      { src = "gui/*.cpp"; }
      { src = "hardware/*.cpp"; libs = [ libpng sdl_net ]; }
      { src = "hardware/serialport/*.cpp"; libs = [ sdl_net ]; }
      { src = "ints/*.cpp"; }
      { src = "misc/*.cpp"; }
      { src = "shell/*.cpp"; }
    ];
}
