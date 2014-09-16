{ build, base, impl }:

{ syscall }:

let
  sourceDir = base.sourceDir + "/platform";
  subdir =
    if build.isArm then sourceDir + "/arm" else
    if build.isx86_32 then sourceDir + "/x86_32" else
    if build.isx86_64 then sourceDir + "/x86_64" else
    abort "no startup library for ${build.spec.system}";
in
build.library rec {
  name = "startup";
  shared = false;

  libs = [ syscall ];

  sources =
    [ (subdir + "/crt0.s")
    ] ++
    map (fn: base.sourceDir + "/platform/${fn}")
      [ "_main.cc"
        "init_main_thread.cc"
      ];

  includeDirs =
    [ "${impl.repo.sourceDir}/platform" sourceDir ]
    ++ impl.repo.includeDirs ++ base.includeDirs;
}