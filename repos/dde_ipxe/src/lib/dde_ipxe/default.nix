{ spec, linkStaticLibrary, compileC, compileCRepo, addPrefix, fromDir, dropSuffix, ipxeSrc
, dde_kit, dde_ipxe_support }:

let
  fromIpxe = fromDir ipxeSrc;

  extraFlags =
    [ "-Wall" "-Wno-address"
      "-fno-builtin-putchar" "-fno-builtin-toupper" "-fno-builtin-tolower"
      "-DARCH=i386" "-DPLATFORM=pcbios" "-include ${ipxeSrc}/include/compiler.h"
      "-Ddebug_lib=7"
    ];

  includes = [ ./include ../../../include ];
  externalIncludes = fromIpxe
    [ ""
      "${ipxeSrc}/include"
      "${ipxeSrc}/arch/x86/include"
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
      (addPrefix "core/" [ "iobuf.c" "string.c" "bitops.c" "list.c" "random.c" ]) ++
      (addPrefix "net/"  [ "ethernet.c" "netdevice.c" "nullnet.c" "eth_slow.c" "iobpad.c" ]) ++
      (addPrefix "drivers/bitbash/" [ "bitbash.c" "spi_bit.c" ]) ++
      (addPrefix "drivers/nvs/" [ "nvs.c" "threewire.c" ]) ++
      (addPrefix "drivers/net/" [ "pcnet32.c" "intel.c" "eepro100.c" "realtek.c" "mii.c" ]);

  libs = [ dde_kit dde_ipxe_support ];
in
linkStaticLibrary {
  name = "dde_ipxe_nic";
  inherit libs;

  objects = map
    (src: compileC { inherit src libs extraFlags includes externalIncludes; })
    [ ./nic.c ./dde.c ./dummies.c ];

  externalObjects = compileCRepo (
    { sources = fromIpxe sources;
      inherit libs extraFlags includes externalIncludes;
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
