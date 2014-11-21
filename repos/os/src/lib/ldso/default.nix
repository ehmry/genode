{ genodeEnv, compileS, compileC, compileCC
, baseLibs, ldso-arch, debug ? true }:

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
  linkAddress = if genodeEnv.isLinux then "0x50000000" else "0x30000";

  dFlags = map (o: "-D${o}") (
    (if genodeEnv.isx86_64 then [ "__ELF_WORD_SIZE=64" ] else []) ++ 
    [ "IN_RTLD" "__BSD_VISIBLE=1" "LINK_ADDRESS=${linkAddress}" ] ++
    (if debug then [ "DEBUG" ] else [])
  );

  archContrib =
    if genodeEnv.isArm    then ./contrib/arm   else
    if genodeEnv.isx86_32 then ./contrib/i386  else
    if genodeEnv.isx86_64 then ./contrib/amd64 else
    throw "ld library expression incomplete for ${genodeEnv.system}";

  archInclude =
    if genodeEnv.isArm    then ./include/libc/libc-arm   else
    if genodeEnv.isx86_32 then ./include/libc/libc-i386  else
    if genodeEnv.isx86_64 then ./include/libc/libc-amd64 else
    throw "ld library expression incomplete for ${genodeEnv.system}";

  addFlags = f: flags: attrs:
    f (attrs // {
      extraFlags = flags;
      localIncludes = [ (archContrib) ./contrib ../ldso ];
      systemIncludes =
        [ archContrib
          archInclude
          ./contrib
          ./include/libc
          ./include/libc_emu
          archInclude
        ];
    });

  extraFlags =
    dFlags ++
    [ "-Iinclude -include ${./include/libc_emu/ldso_types.h}"
      "-Wno-unused-but-set-variable"
    ];

  ccFlags = extraFlags ++ [ "-fno-builtin" ];

  compileS'  = addFlags compileS  ccFlags;
  compileC'  = addFlags compileC  ccFlags;
  compileCC' = addFlags compileCC extraFlags;

in
genodeEnv.mkLibrary {
  name = "ld";
  shared = true;
  libs = baseLibs ++ [ ldso-arch ];

  entryPoint = "_start";
  asOpt = genodeEnv.asOpt ++ dFlags;
  ldOpt = genodeEnv.ldOpt ++
    [ "-Bsymbolic-functions"
      "-T${./ldso.ld} --version-script=${./symbol.map}"
    ] ++
    #
    # Add context area script to Linux version of linker
    #
    (if genodeEnv.isLinux then [
      "-T${../../../../base-linux/src/platform/context_area.nostdlib.ld}"
    ] else []);

  objects =
    [ (compileS' { src = archContrib+"/rtld_start.S"; })
      (compileC' { src = archContrib+"/reloc.c"; })
    ] ++
    map (src: compileC' { inherit src; })
      [ ./contrib/rtld.c ./contrib/map_object.c
        ./contrib/xmalloc.c ./contrib/debug.c
      ]
    ++
    map (src: compileC' { inherit src; })
      [ ./main.c ./ldso_types.c ./rtld_dummies.c
        (if genodeEnv.isArm then ./arm/platform.c else ./platform.c)
      ]
    ++
    map (src: compileCC' { inherit src; })
      [ ./stdio.cc ./stdlib.cc ./file.cc ./err.cc ./string.cc ./lock.cc
        ./test.cc ./environ.cc ./call_program_main.cc
      ];
}
