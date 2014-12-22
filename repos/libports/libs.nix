/*
 * \brief  Libports libraries
 * \author Emery Hemingway
 * \date   2014-09-20
 */

{ spec, tool, callLibrary }:

let

  # The port source expressions.
  ports = import ./ports { inherit tool; };
  # Append 'Src' to each attribute in ports.
  ports' = builtins.listToAttrs (
    map
      (n: { name = n+"Src"; value = builtins.getAttr n ports; })
      (builtins.attrNames ports)
  );

  libcArchInclude =
    if tool.genodeEnv.isArm    then "libc-arm"    else
    if tool.genodeEnv.isx86_32 then "libc-i386"  else
    if tool.genodeEnv.isx86_64 then "libc-amd64" else
    throw "no libc for ${tool.genodeEnv.system}";

  libcIncludes =
    [ ./include/libc-genode ] ++
    ( map
        (d: "${ports.libc.include}/"+d)
        [ "libc" libcArchInclude ]
    );

  compileLibc =
  { sourceRoot ? ports.libc
  , sources
  , extraFlags ? []
  , localIncludes ? []
  , systemIncludes ? []
  , ... } @ args:
  compileCRepo (args // {
    inherit sourceRoot sources;
    extraFlags = extraFlags ++
      [ # Generate position independent code to allow linking of
        # static libc code into shared libraries
        # (define is evaluated by assembler files)
        "-DPIC"

        # Prevent gcc headers from defining __size_t.
        # This definition is done in machine/_types.h.
        "-D__FreeBSD__=8"

        # Prevent gcc-4.4.5 from generating code for the family of
        # 'sin' and 'cos' functions because the gcc-generated code
        # would actually call 'sincos' or 'sincosf', which is a GNU
        # extension, not provided by our libc.
        "-fno-builtin-sin" "-fno-builtin-cos"
        "-fno-builtin-sinf" "-fno-builtin-cosf"
      ];


    localIncludes = localIncludes ++ [ "lib/libc/include" ];

    systemIncludes = systemIncludes ++ libcIncludes;

  });

 compileCC =
  attrs:
  tool.compileCC (attrs // {
    systemIncludes =
     (attrs.systemIncludes or []) ++
      ( import ../base/include { inherit (tool) genodeEnv; }) ++
      [ ./include ];
  });

 compileCRepo =
  attrs:
  tool.compileCRepo (attrs // {
    systemIncludes =
     (attrs.systemIncludes or []) ++
      ( import ../base/include { inherit (tool) genodeEnv; }) ++
      [ ./include ];
  });

  callLibrary' = callLibrary (
    { inherit (tool) genodeEnv compileCCRepo;
      inherit compileLibc libcIncludes compileCC compileCRepo;
    } // ports'
  );
  importLibrary = path: callLibrary' (import path);

in
{
  gmp-mpn = importLibrary ./src/lib/gmp/mpn.nix;
  icu     = importLibrary ./lib/mk/icu.nix;
  libc-compat  = importLibrary ./lib/mk/libc-compat.nix;
  libc-locale  = importLibrary ./lib/mk/libc-locale.nix;
  libc-gen     = importLibrary ./lib/mk/libc-gen.nix;
  libc-gdtoa   = importLibrary ./lib/mk/libc-gdtoa.nix;
  libc-inet    = importLibrary ./lib/mk/libc-inet.nix;
  libc-isc     = importLibrary ./lib/mk/libc-isc.nix;
  libc-nameser = importLibrary ./lib/mk/libc-nameser.nix;
  libc-net     = importLibrary ./lib/mk/libc-net.nix;
  libc-regex   = importLibrary ./lib/mk/libc-regex.nix;
  libc-resolv  = importLibrary ./lib/mk/libc-resolv.nix;
  libc-rpc     = importLibrary ./lib/mk/libc-rpc.nix;
  libc-stdlib  = importLibrary ./lib/mk/libc-stdlib.nix;
  libc-stdtime = importLibrary ./lib/mk/libc-stdtime.nix;
  libc-setjmp  = importLibrary ./lib/mk/libc-setjmp.nix;
  libc-string  = importLibrary ./lib/mk/libc-string.nix;
  libc-stdio   = importLibrary ./lib/mk/libc-stdio.nix;
  libc = importLibrary ./src/lib/libc;
  libm = importLibrary ./lib/mk/libm.nix;

  libc_lock_pipe     = importLibrary ./src/lib/libc_lock_pipe;
  libc_lwip          = importLibrary ./src/lib/libc_lwip;
  libc_lwip_nic_dhcp = importLibrary ./src/lib/libc_lwip_nic_dhcp;

  libpng  = importLibrary ./src/lib/libpng;
  lwip    = importLibrary ./src/lib/lwip;
  pthread = importLibrary ./src/lib/pthread;
  sdl     = importLibrary ./src/lib/sdl;
  sdl_net = importLibrary ./src/lib/sdl_net;
  stdcxx  = importLibrary ./lib/mk/stdcxx.nix;
  zlib    = importLibrary ./lib/mk/zlib.nix;

  test-ldso_lib_1  = importLibrary ./src/test/ldso/lib_1.nix;
  test-ldso_lib_2  = importLibrary ./src/test/ldso/lib_2.nix;
  test-ldso_lib_dl = importLibrary ./src/test/ldso/lib_dl.nix;

  x86emu = importLibrary ./lib/mk/x86emu.nix;
}
