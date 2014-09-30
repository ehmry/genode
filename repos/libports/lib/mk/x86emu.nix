{ tool }: with tool;

let
  portDir = "${libports.port.x86emu}/x86emu";
in
buildLibrary {
  name = "x86emu";
  flags = [ "-fomit-frame-pointer" ];
  sources = map (fn: "${portDir}/${fn}")
    [ "decode.c" "fpu.c" "ops.c" "ops2.c" "prim_ops.c" "sys.c" ];
  includeDirs =
    [ portDir
      (libports.includeDir + "/x86emu")
    ] ++ base.includeDirs;
}
