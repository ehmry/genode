{ spec, linkComponent, transformBinary
, compileCC, compileCCRepo, seoulSrc
, base, blit, alarm, seoul_libc_support, config }:

let
  libs = [ base blit alarm seoul_libc_support config ];

  systemIncludes =
    [ "${seoulSrc}/include"
      "${seoulSrc}/genode/include"
      ../../../include
      ./include
    ];

  extraFlags =
    [ (if spec.is64Bit then "-mcmodel=large" else "")
      "-march=core2" "-msse3"
      #"-D__FreeBSD__=8 -fno-builtin-sin -fno-builtin-cos -fno-builtin-sinf -fno-builtin-cosf"
      "-Wno-parentheses" "-Wall"
    ];

  PIC = false;

  compileCC' = src: compileCC {
    inherit src libs systemIncludes extraFlags PIC;
  };
in
if spec.isNOVA then linkComponent {
  name = "seoul";
  inherit libs;

  ldTextAddr =
    if spec.is32Bit then "0xbf800000" else
    if spec.is64Bit then "0x7fffff800000" else
    throw "S{spec.system} is not supported by seoul";

  objects =
    # Nix copies symlinks verbatim to the store,
    # so ./mono.tff wouldn't work here.
    [ (transformBinary ../../../../demo/src/server/nitlog/mono.tff) ] ++
    (map
      compileCC'
      [ ./main.cc ./nova_user_env.cc ./device_model_registry.cc
        ./console.cc ./keyboard.cc ./network.cc ./disk.cc
      ]
    );

  externalObjects = compileCCRepo {
    sourceRoot = seoulSrc;
    sources = [ "model/*.cc" "executor/*.cc" ];
    inherit libs systemIncludes extraFlags PIC;
  };

} else null
