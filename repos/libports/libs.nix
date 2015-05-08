/*
 * \brief  Libports libraries
 * \author Emery Hemingway
 * \date   2014-09-20
 */

{ spec, tool, callLibrary
, baseIncludes, osIncludes, libportsIncludes, ... }:

let

  # Append 'Src' to each attribute in ports.
  ports =
    let p = import ./ports { inherit tool; }; in
    builtins.listToAttrs (map
      (n: { name = n+"Src"; value = builtins.getAttr n p; })
      (builtins.attrNames p)
  );

  fromLibc = tool.fromDir ports.libcSrc;

  libcArchInclude =
    if spec.isArm    then "libc-arm"    else
    if spec.isx86_32 then "libc-i386"  else
    if spec.isx86_64 then "libc-amd64" else
    throw "no libc for ${spec.system}";

  libcIncludes =
    [ ./include/libc-genode ];

  libcExternalIncludes = tool.fromDir
    ports.libcSrc.include
    [ "libc" libcArchInclude ];

  compileLibc =
  { sources
  , filter ? []
  , extraFlags ? []
  , includes ? []
  , externalIncludes ? []
  , ... } @ args:
  compileCRepo (args // {
    # Make sources absolute to the libc source.
    sources = fromLibc sources;
    filter = fromLibc filter;

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

    includes = includes ++ libcIncludes ++ [ ./src/lib/libc ];
    externalIncludes = externalIncludes ++ [ "${ports.libcSrc}/lib/libc/include" ] ++ libcExternalIncludes;
  });

  addIncludes =
  f: attrs:
  f (attrs // {
    includes =
     (attrs.includes or []) ++ libportsIncludes ++ osIncludes ++ baseIncludes;
  });

 compileCC = addIncludes tool.compileCC;
 compileCRepo = addIncludes tool.compileCRepo;

 callLibrary' = callLibrary
    ( { inherit fromLibc compileLibc libcIncludes libcExternalIncludes compileCC compileCRepo; } // ports );

  importLibrary = path: callLibrary' (import path);

in
{
  seoul_libc_support = importLibrary
    ./../ports/lib/mk/seoul_libc_support.nix;

  test-ldso_lib_1  = importLibrary ./src/test/ldso/lib_1.nix;
  test-ldso_lib_2  = importLibrary ./src/test/ldso/lib_2.nix;
  test-ldso_lib_dl = importLibrary ./src/test/ldso/lib_dl.nix;
} // (tool.loadExpressions callLibrary' ./src/lib)
  // (tool.loadExpressions callLibrary' ./lib/mk)
