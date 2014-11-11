{ genodeEnv, baseLibs, ldso-arch, debug ? true }:

let

  #
  # Change link address
  #
  # When LINK_ADDRESS does not match the memory address of ld.lib.so
  # at runtime, GDB shows the following message when attaching to a
  # dynamically linked Genode process (for example test-lwip_httpsrv):
  #
  # "warning: .dynamic section for ".../libcache/ld/ld.lib.so" is 
  # not at the expected address (wrong library or version mismatch?)"
  #
  # In this case, backtraces show no symbol names or source lines.
  #
  linkAddress =
    if genodeEnv.isLinux then "0x50000000" else "0x30000";

  dOpts = map (o: "-D${o}") (
    [ "IN_RTLD" "__BSD_VISIBLE=1" "LINK_ADDRESS=${linkAddress}"
      "GNU_SOURCE"
    ]
    ++ (if genodeEnv.isx86_64 then [ "__ELF_WORD_SIZE=64" ] else [])
    ++ (if debug then [ "DEBUG" ] else [])
  );

  dir = ../ldso;

  incomplete = "ld library expression incomplete for ${genodeEnv.system}";
  
  archAttrs =
    if genodeEnv.isArm then
      { sources = genodeEnv.fromPath ./arm/platform.c; }
    else
    if genodeEnv.isx86_64 then
      { #incDir =
        #  [ (dir+"/contrib/amd64")
        #    (dir+"/include/libc/libc-amd64")
        #   #(dir+"/include/x86_64")
        #  ];
        sources = genodeEnv.fromPath ./platform.c;
      } 
    else throw incomplete;

  archIncDir =
    if genodeEnv.isx86_64
    then ./include/libc/libc-amd64
    else throw incomplete;
  
  archContribDir =
    if genodeEnv.isx86_64 then ./contrib/amd64 else throw incomplete;

in
genodeEnv.mkLibrary (genodeEnv.tool.mergeSets [ archAttrs {
  name = "ld";
  shared = true;
  libs = baseLibs ++ [ ldso-arch ];

  sources =
    genodeEnv.fromDir archContribDir [ "rtld_start.S" "reloc.c" ]
    ++
    genodeEnv.fromDir ./contrib
      [ "rtld.c" "map_object.c" "xmalloc.c" "debug.c" ]
    ++
    genodeEnv.fromDir ../ldso
      [ "main.c"
        "ldso_types.c" "rtld_dummies.c"
        "stdio.cc" "stdlib.cc" "file.cc" "err.cc" "string.cc" "lock.cc"
        "test.cc" "environ.cc" "call_program_main.cc"
      ];

  localIncludes = [ ../ldso ];

  systemIncludes =
    [ archIncDir
      archContribDir
      ./include/libc
      ./include/libc_emu
    ];

  entryPoint = "_start";

  ccDef = 
    dOpts ++ 
    [ "-fno-builtin" 
      "-Iinclude -include ${dir}/include/libc_emu/ldso_types.h"
    ];

  cxxDef = dOpts;
  asOpt  = dOpts;

  ldOpt =
    [ "-Bsymbolic-functions"  
      "-T${dir}/ldso.ld --version-script=${dir}/symbol.map"
      
      #
      # Add context area script to Linux version of linker
      #
      ( if genodeEnv.isLinux then
          "-T${../../../../base-linux/src/platform/context_area.nostdlib.ld}"
        else ""
      )
    ];

  ccOpt =
    [ "-Wno-unused-but-set-variable" ] ++ genodeEnv.ccOpt;

}])
