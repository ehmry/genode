#
# x86 real-mode emulation library
#

{ linkStaticLibrary, compileCRepo, x86emuSrc }:

let
  includeDir = x86emuSrc.include;
  systemIncludes = [ includeDir ../../include/x86emu ];
in
linkStaticLibrary {
  name = "x86emu";

  externalObjects = compileCRepo {
    extraFlags = [ "-fomit-frame-pointer" "-I." ];
    sourceRoot = x86emuSrc;
    sources =
      [ "decode.c" "fpu.c" "ops.c" "ops2.c" "prim_ops.c" "sys.c" ];
    localIncludes  = [ includeDir ];
    inherit systemIncludes;
  };

  propagate = { inherit systemIncludes; };
}
