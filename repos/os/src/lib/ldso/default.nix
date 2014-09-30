/*
 * \author Emery Hemingway
 * \date   2014-09-22
 */

{ tool }: with tool;
{ base-common, base, startup, cxx
, timed_semaphore, alarm, config, syscall, ldso-arch }:


let
  srcDir = ../ldso;

  archSrcDir =
    if tool.build.isx86_32 then "${srcDir}/contrib/i386" else
    if tool.build.isx86_64 then "${srcDir}/contrib/amd64" else
    if tool.build.isxArm   then "${srcDir}/contrib/arm" else
    throw "no ld.lib.so for ${tool.build.system}";

  archIncDir =
    if tool.build.isx86_32 then [
      "${srcDir}/contrib/i386"
       "${srcDir}/include/libc/libc-i386"
    ] else
    if tool.build.isx86_64 then [
       "${srcDir}/contrib/amd64"
       "${srcDir}/include/libc/libc-amd64"
    ] else
    if tool.build.isxArm then [
     "${srcDir}/contrib/arm"
     "${srcDir}/include/libc/libc-arm"
    ] else throw "no ld.lib.so for ${tool.build.system}";

  #
  # Change link address
  #
  # When LINK_ADDRESS does not match the memory address of ld.lib.so at runtime,
  # GDB shows the following message when attaching to a dynamically linked Genode
  # process (for example test-lwip_httpsrv):
  #
  # "warning: .dynamic section for ".../libcache/ld/ld.lib.so" is not at the
  #  expected address (wrong library or version mismatch?)"
  #
  # In this case, backtraces show no symbol names or source lines.
  #
  linkAddress =
    if tool.build.isLinux then "0x50000000" else "0x30000";

  dOpts = map (o: "-D"+o) (
    (if tool.build.isx86_64 then [ "__ELF_WORD_SIZE=64" ] else [])
    ++
    [ "IN_RTLD" "__BSD_VISIBLE=1"
      "LINK_ADDRESS=${linkAddress}"
    ]
    #++
    #(if debug then [ "DEBUG" ] else [])
  );

in
buildLibrary {
  name = "ld";
  shared = true;

  libs =
    [ base-common base startup cxx
      timed_semaphore alarm config syscall ldso-arch
    ];

  ccDef  = dOpts ++ [
    "-fno-builtin" "-Iinclude" "
    -include ${srcDir}/include/libc_emu/ldso_types.h"
  ];
  cxxDef = dOpts;
  asOpt  = dOpts;
  ccOpt  = [ "-Wno-unused-but-set-variable" ] ++ tool.build.ccOpt;
  ldOpt =
    [ "-Bsymbolic-functions" "-T${srcDir}/ldso.ld"
      "--version-script=${srcDir}/symbol.map"
      ( if tool.build.isLinux then
          "-T${../../../../base-linux/src/platform/context_area.nostdlib.ld}"
        else null
      )
    ] ++ tool.build.ldOpt;

  entryPoint = "_start";

  sources =
    [ "${archSrcDir}/rtld_start.S" "${archSrcDir}/reloc.c" ]
    ++
    ( map (fn: "${srcDir}/${fn}")
        [ "ldso_types.c" "rtld_dummies.c" "platform.c"
          "stdio.cc" "file.cc" "err.cc" "string.cc" "lock.cc"
          "test.cc" "environ.cc" "call_program_main.cc"
        ]
    )
    ++
    ( map (fn: "${./contrib}/${fn}")
        [ "rtld.c" "map_object.c" "xmalloc.c" "debug.c" ]
    );

  includeDirs =
    [ srcDir
      "${srcDir}/contrib"
      "${srcDir}/include/libc"
      "${srcDir}/include/libc_emu"
    ] ++ archIncDir;

}

