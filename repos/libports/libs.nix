/*
 * \brief  Libports libraries
 * \author Emery Hemingway
 * \date   2014-09-20
 */

{ tool, callLibrary
, baseIncludes, osIncludes, libportsIncludes }:

let
  ports = import ./ports { inherit tool; };
  tool' = tool // { inherit buildLibrary buildLibraryPlain; };

  callWithPorts = f:
    f (builtins.intersectAttrs (builtins.functionArgs f) ports);
  importLibrary = path:
    callLibrary (callWithPorts (
      import path { tool = tool'; }
    ));

  importLibcLibrary = path:
    import path { tool = tool'; inherit (ports) libc; };

  buildLibraryPlain = { includeDirs ? [], ... } @ args:
    tool.build.library (args // {
      includeDirs =  builtins.concatLists [
        includeDirs osIncludes baseIncludes
      ];
    });


  # overide the build.library function
  buildLibrary = { ccOpt ? [], includeDirs ? [], ... } @ args:
    tool.build.library (args // {

      #
      # Use default warning level rather than -Wall because we do not want to touch
      # the imported source code to improve build aesthetics
      #
      ccWarn = null;

      ccOpt = ccOpt ++
        [ #
          # Generate position independent code to allow linking of static libc code into
          # shared libraries (define is evaluated by assembler files)
          #
          "-DPIC"

          #
          # Prevent gcc headers from defining __size_t. This definition is done in
          # machine/_types.h.
          #
          "-D__FreeBSD__=8"

          #
          # Prevent gcc-4.4.5 from generating code for the family of 'sin' and 'cos'
          # functions because the gcc-generated code would actually call 'sincos'
          # or 'sincosf', which is a GNU extension, not provided by our libc.
          #
          "-fno-builtin-sin" "-fno-builtin-cos" "-fno-builtin-sinf" "-fno-builtin-cosf"
        ] ++ tool.build.ccOpt;

      includeDirs =  builtins.concatLists [
        includeDirs
        ( map (d: "${ports.libc}/src/lib/libc/${d}")
            [ "lib/libc/locale"
              "lib/libc/include"
              "lib/libc/stdio"
              "lib/libc/net"
              "contrib/gdtoa"
            ] )
        [ "${ports.libc}/include/libc"
          #
          # Genode-specific supplements to standard libc headers
          #
          include/libc-genode

          ( if tool.build.isx86_32 then "${ports.libc}/include/libc-i386" else
            if tool.build.isx86_64 then "${ports.libc}/include/libc-amd64" else
            if tool.build.isxArm   then "${ports.libc}/include/libc-arm" else
            throw "libc port does not support ${tool.build.system}" )
        ]
        libportsIncludes osIncludes baseIncludes
      ];
    });

  genPath =
    if tool.build.isx86_32 then ./lib/mk/x86_32/libc-gen.nix else
    if tool.build.isx86_64 then ./lib/mk/x86_64/libc-gen.nix else
    if tool.build.isArm then ./lib/mk/arm/libc-gen.nix else
    abort "no libc-gen library for ${tool.build.system}";

  setjmpPath =
    if tool.build.isx86_32 then ./lib/mk/x86_32/libc-setjmp.nix else
    if tool.build.isx86_64 then ./lib/mk/x86_64/libc-setjmp.nix else
    if tool.build.isArm then ./lib/mk/arm/libc-setjmp.nix else
    abort "no libc-setjmp library for ${tool.build.system}";

in
{
  libc-compat  = importLibcLibrary ./lib/mk/libc-compat.nix;
  libc-locale  = importLibcLibrary ./lib/mk/libc-locale.nix;
  libc-gen     = importLibcLibrary genPath;
  libc-gdtoa   = importLibcLibrary ./lib/mk/libc-gdtoa.nix;
  libc-inet    = importLibcLibrary ./lib/mk/libc-inet.nix;
  libc-regex   = importLibcLibrary ./lib/mk/libc-regex.nix;
  libc-stdlib  = importLibcLibrary ./lib/mk/libc-stdlib.nix;
  libc-stdtime = importLibcLibrary ./lib/mk/libc-stdtime.nix;
  libc-setjmp  = importLibcLibrary setjmpPath;
  libc-string  = importLibcLibrary ./lib/mk/libc-string.nix;
  libc-stdio   = importLibcLibrary ./lib/mk/libc-stdio.nix;
  libc = callLibrary (importLibcLibrary ./lib/mk/libc.nix);
  libm = callLibrary (importLibcLibrary ./lib/mk/libm.nix);

  libpng = importLibrary ./lib/mk/libpng.nix;
  zlib   = importLibrary ./lib/mk/zlib.nix;

  test-ldso_lib_1 = importLibrary ./lib/mk/test-ldso_lib_1.nix;
  test-ldso_lib_2 = importLibrary ./lib/mk/test-ldso_lib_2.nix;
}