{ spec, linkStaticLibrary, compileC, compileCRepo, dropSuffix, ipxeSrc
, dde_kit, dde_ipxe_support }:

let
  extraFlags =
    [ "-Wall" "-Wno-address"
      "-fno-builtin-putchar" "-fno-builtin-toupper" "-fno-builtin-tolower"
      "-DARCH=i386" "-DPLATFORM=pcbios" "-include ${ipxeSrc}/include/compiler.h"
      "-Ddebug_lib=7"
    ];

  systemIncludes =
    [ ./include
      "${ipxeSrc}/include"
      ipxeSrc
      "${ipxeSrc}/arch/x86/include"
      ../../../include
    ] ++
    ( if spec.isi686 then
        [ "${ipxeSrc}/arch/i386/include"
          "${ipxeSrc}/arch/i386/include/pcbios"
        ] else
      if spec.isx86_64 then
        [ "${ipxeSrc}/arch/x86_64/include"
          "${ipxeSrc}/arch/x86_64/include/efi"
          "${ipxeSrc}/arch/i386/include"

        ] else
      throw "no dde_ipxe_nic expression for ${spec.system}"
    );

    sources =
      [ "arch/x86/core/x86_string.c"
        "arch/i386/core/rdtsc_timer.c"
        "drivers/bus/pciextra.c"
      ] ++
      (map (f:"core/"+f) [ "iobuf.c" "string.c" "bitops.c" "list.c" "random.c" ]) ++
      (map (f:"net/"+f)  [ "ethernet.c" "netdevice.c" "nullnet.c" "eth_slow.c" "iobpad.c" ]) ++
      (map (f:"drivers/bitbash/"+f) [ "bitbash.c" "spi_bit.c" ]) ++
      (map (f:"drivers/nvs/"+f) [ "nvs.c" "threewire.c" ]) ++
      (map (f:"drivers/net/"+f) [ "pcnet32.c" "intel.c" "eepro100.c" "realtek.c" "mii.c" ]);

in
linkStaticLibrary rec {
  name = "dde_ipxe_nic";
  libs = [ dde_kit dde_ipxe_support ];

  objects = map
    (src: compileC { inherit src libs extraFlags systemIncludes; })
    [ ./nic.c ./dde.c ./dummies.c ];

  externalObjects = compileCRepo (
    { sourceRoot = ipxeSrc;
      inherit sources libs extraFlags systemIncludes;
    } // builtins.listToAttrs (
      map
        (s:
          let bn = dropSuffix ".c" (baseNameOf s); in
          { name = "ccFlags_"+bn; value = "-DOBJECT="+bn; }
        )
        sources
    )
  );

  propagate.systemIncludes = [ ../../../include ];
}
