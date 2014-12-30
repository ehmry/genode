{ genodeEnv, linkComponent, compileCCRepo, dosboxSrc
, libc, libm, libpng, sdl, sdl_net, stdcxx, zlib, libc_lwip_nic_dhcp
, config_args }:

linkComponent rec {
  name = "dosbox";

  libs =
    [ libc libm libpng sdl sdl_net stdcxx
      zlib libc_lwip_nic_dhcp config_args
    ];

  externalObjects = compileCCRepo {
    inherit name libs;
    sourceRoot = dosboxSrc;
    filter = [ "opl.cpp" ];
    sources = map
      (glob: "src/${glob}")
      [ "dosbox.cpp"
        "cpu/*.cpp" "debug/*.cpp" "dos/*.cpp" "fpu/*.cpp" "gui/*.cpp"
        "hardware/*.cpp" "hardware/serialport/*.cpp" "ints/*.cpp"
        "misc/*.cpp" "shell/*.cpp"
      ];

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

    localIncludes = [ "include" ];

    systemIncludes =
      [ ../dosbox ] ++
      ( if genodeEnv.isx86_32 then [ ./x86_32 ] else
        if genodeEnv.isx86_64 then [ ./x86_64 ] else
        [ ]
      );
  };

}
