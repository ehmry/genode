/*
 * \brief  Libports libraries
 * \author Emery Hemingway
 * \date   2014-09-20
 */

{ tool, callLibrary }:

let

  # The port source expressions.
  ports = import ./ports { inherit tool; };
  # Append 'Src' to each attribute in ports.
  ports' = builtins.listToAttrs (
    map
      (n: { name = n+"Src"; value = builtins.getAttr n ports; })
      (builtins.attrNames ports)
  );

  ##
  ## Automatically add the base library for targets that use the libc
  ##
  ## We test for an empty 'LIB_MK' variable to determine whether we are currently
  ## processing a library or a target. We only want to add the base library to
  ## targets, not libraries.
  ##
  #ifeq ($(LIB_MK),)
  #LIBS += base
  #endif

  libcArchInclude =
    if tool.genodeEnv.isArm    then "include/libc-arm"    else
    if tool.genodeEnv.isx86_32 then "include/libc-i386"  else
    if tool.genodeEnv.isx86_64 then "include/libc-amd64" else
    throw "no libc for ${tool.genodeEnv.system}";

  compileLibc =
  args:
  derivation (tool.genodeEnv.stdAttrs // args // {
    name = (args.name or "libc")+"-objects";
    system = builtins.currentSystem;
    builder = tool.shell;
    args = [ "-e" ./compile-libc.sh ];
    inherit (tool) genodeEnv;

    sourceRoot = args.sourceRoot or ports.libc;

    # Use default warning level rather than -Wall because we do
    # not want to touch
    # the imported source code to improve build aesthetics
    # ccWarn = null;
    # ^ has no effect

    ccFlags = args.ccFlags or [] ++ [
      # Generate position independent code to allow linking of
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
    ]
    ++ tool.genodeEnv.ccFlags;

    localIncludes = (args.localIncludes or []) ++
      [ "lib/libc/include" ];

    systemIncludes = (args.systemIncludes or []) ++
      #map (p: "${ports.libc}/${p}")
        [ "include/libc"
          "lib/libc/include"
          # Add platform-specific libc headers
          # to standard include search paths
          libcArchInclude
        ]
      ++ [ ./include/libc-genode ];
  });

  # Function to compile the sub-libraries of our libc.
  compileSubLibc = args:
    compileLibc (args // {
      localIncludes = args.localIncludes or [] ++
        [ "lib/libc/locale"
          "lib/libc/include"
          "lib/libc/stdio"
          "lib/libc/net"
          "contrib/gdtoa"
          "sys"
          libcArchInclude
        ];
      });

 compileCC =
  attrs:
  tool.compileCC (attrs // {
    systemIncludes =
     (attrs.systemIncludes or []) ++
      ( import ../base/include { inherit (tool) genodeEnv; }) ++
      [ ./include ];
  });

  callLibrary' = callLibrary (
    { inherit (tool) genodeEnv;
      inherit compileLibc compileSubLibc compileCC;
    } // ports'
  );
  importLibrary = path: callLibrary' (import path);

in
{
  libc-compat  = importLibrary ./lib/mk/libc-compat.nix;
  libc-locale  = importLibrary ./lib/mk/libc-locale.nix;
  libc-gen     = importLibrary ./lib/mk/libc-gen.nix;
  libc-gdtoa   = importLibrary ./lib/mk/libc-gdtoa.nix;
  libc-inet    = importLibrary ./lib/mk/libc-inet.nix;
  libc-regex   = importLibrary ./lib/mk/libc-regex.nix;
  libc-stdlib  = importLibrary ./lib/mk/libc-stdlib.nix;
  libc-stdtime = importLibrary ./lib/mk/libc-stdtime.nix;
  libc-setjmp  = importLibrary ./lib/mk/libc-setjmp.nix;
  libc-string  = importLibrary ./lib/mk/libc-string.nix;
  libc-stdio   = importLibrary ./lib/mk/libc-stdio.nix;

  libc = importLibrary ./lib/mk/libc.nix;
  libm = importLibrary ./lib/mk/libm.nix;

  #libpng = importLibrary ./lib/mk/libpng.nix;
  #zlib   = importLibrary ./lib/mk/zlib.nix;

  test-ldso_lib_1 = importLibrary ./lib/mk/test-ldso_lib_1.nix;
  test-ldso_lib_2 = importLibrary ./lib/mk/test-ldso_lib_2.nix;
}
