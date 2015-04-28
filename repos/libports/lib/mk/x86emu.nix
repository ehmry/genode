#
# x86 real-mode emulation library
#

{ linkStaticLibrary, compileCRepo, fromDir, x86emuSrc }:

let
  externalIncludes =
    [  ../../include/x86emu x86emuSrc.include "${x86emuSrc.include}/x86emu" ];
  headers = [ ../../include/x86emu/sys/types.h ];
in
linkStaticLibrary {
  name = "x86emu";

  externalObjects = compileCRepo {
    inherit headers;
    extraFlags = [ "-fomit-frame-pointer" "-I." ];
    sources = fromDir x86emuSrc
      [ "decode.c" "fpu.c" "ops.c" "ops2.c" "prim_ops.c" "sys.c" ];
    externalIncludes = [ x86emuSrc ] ++ externalIncludes;
  };

  propagate = { inherit externalIncludes; };
}
