{ tool, build, dosbox }:

{ libc, libm, libpng, sdl, sdl_net, stdcxx, zlib, libc_lwip_nic_dhcp, config_args }:

let
  srcDirs = map (d: "${dosbox}/src/${d}") [
    "cpu" "debug" "dos" "fpu" "gui" "hardware" "hardware/serialport"
  ];
in
build.component {
  name = "dosbox";
  libs = [ libc libm libpng sdl sdl_net stdcxx zlib libc_lwip_nic_dhcp config_args ];

  sources = builtins.concatLists ([
    [ "${dosbox}/src/dosbox.cpp" ]
  ] ++ map (d: tool.wildcard "${d}/*.cpp") srcDirs);

  includeDirs =
    [ ../dosbox
      ( if build.isx86_32 then ./x86_32 else
        if build.isx86_64 then ./x86_64 else
        null
      )
      "${dosbox}/include"
    ] ++ srcDirs;

  ccOpt =
    [ "-DHAVE_CONFIG_H" "-D_GNU_SOURCE=1" "-D_REENTRANT"
      ( if build.isx86_32 then "-DC_TARGETCPU=X86" else
        if build.isx86_64 then "-DC_TARGETCPU=X86_64" else
        null
      )
    ];

  ccWarn =
    [ "-Wall"
      "-Wno-unused-variable" "-Wno-unused-function" "-Wno-switch" "-Wno-unused-value"
      "-Wno-unused-but-set-variable" "-Wno-format" "-Wno-maybe-uninitialized"
      "-Wno-sign-compare" "-Wno-narrowing" "-Wno-missing-braces" "-Wno-array-bounds"
      "-Wno-parentheses"
    ];
}