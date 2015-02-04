#
# x86 real-mode emulation library
#

{ linkStaticLibrary, compileCRepo, addPrefix, x86emuSrc }:

let
  externalIncludes =
    [  ../../include/x86emu x86emuSrc.include "${x86emuSrc.include}/x86emu" ];
in
linkStaticLibrary {
  name = "x86emu";

  externalObjects = compileCRepo {
    extraFlags = [ "-fomit-frame-pointer" "-I." ];
    sources = addPrefix "${x86emuSrc}/"
      [ "decode.c" "fpu.c" "ops.c" "ops2.c" "prim_ops.c" "sys.c" ];
    externalIncludes = [ x86emuSrc ] ++ externalIncludes;
  };

  propagate = { inherit externalIncludes; };
}
