{ build, base, os, libports }:

let
  portDir = "${libports.port.x86emu}/x86emu";
in
build.library {
  name = "x86emu";
  flags = [ "-fomit-frame-pointer" ];
  sources = map (fn: "${portDir}/${fn}")
    [ "decode.c" "fpu.c" "ops.c" "ops2.c" "prim_ops.c" "sys.c" ];
  includeDirs =
    [ portDir
      (libports.includeDir + "/x86emu")
    ] ++ base.includeDirs;
}
