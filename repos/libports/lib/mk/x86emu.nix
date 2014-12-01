#
# x86 real-mode emulation library
#

{ genodeEnv, compileCPort, x86emuSrc }:

let
  includeDir = x86emuSrc + "/include/x86emu";
in
genodeEnv.mkLibrary {
  name = "x86emu";

  externalObjects = compileCPort {
    extraFlags = [ "-fomit-frame-pointer" "-I." ];
    sourceRoot = x86emuSrc;
    sources =
      [ "decode.c" "fpu.c" "ops.c" "ops2.c" "prim_ops.c" "sys.c" ];
    localIncludes  = [ includeDir ];
    systemIncludes = [ includeDir ../../include/x86emu ];
  };

  propagatedIncludes =
    [ (x86emuSrc + "/include") ../../include/x86emu ];
}
