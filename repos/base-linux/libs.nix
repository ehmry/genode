/*
 * \brief  Libraries for the Linux implementation of Base.
 * \author Emery Hemingway
 * \date   2014-09-11
 */

{ build, callLibrary, base }:

let

  syscallPath =
    if build.isArm then ./lib/mk/arm/syscall.nix else
    if build.isx86_32 then ./lib/mk/x86_32/syscall.nix else
    if build.isx86_64 then ./lib/mk/x86_64/syscall.nix else
    throw "syscall library unavailable for ${build.spec}";

  baseRepo = base;

in rec {

  repo = rec {
    sourceDir = ./src;
    includeDir = ./include;
    includeDirs = [ includeDir ./src/platform ];
  };

  # these get overridden when merged with base
  base = import ./lib/mk/base.nix { inherit repo; };
  base-common = import ./lib/mk/base-common.nix { inherit repo; };

  syscall = build.library ({
    name = "syscall";
    includeDirs = [];
  } // import syscallPath { inherit (repo) sourceDir; });

  lx_hybrid = callLibrary (
    { base-common, syscall, cxx }:
    build.library {
      name = "lx_hybrid";
      libs = [ base-common syscall cxx ]; # from lib/mk/base.inc
      sources =
        [ ./src/platform/lx_hybrid.cc
          (baseRepo.sourceDir + "/base/cxx/new_delete.cc")
        ] ++
        # from lib/mk/base.inc
        [ (baseRepo.sourceDir + "/base/console/log_console.cc")
          (baseRepo.sourceDir + "/base/env/env.cc")
          ./src/base/env/platform_env.cc
          (baseRepo.sourceDir + "/base/env/context_area.cc")
        ];
      includeDirs = [ ./src/base/env (baseRepo.sourceDir + "/base/env") ] ++ baseRepo.includeDirs;
    }
  );

}