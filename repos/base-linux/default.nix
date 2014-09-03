/*
 * \brief  Expression for the Linux implementation of base
 * \author Emery Hemingway
 * \date   2014-08-12
 */

{ tool, base }:

let
  inherit (tool) build;

  syscallPath =
    if build.isArm then ./lib/mk/arm/syscall.nix else
    if build.isx86_32 then ./lib/mk/x86_32/syscall.nix else
    if build.isx86_64 then ./lib/mk/x86_64/syscall.nix else
    throw "syscall library unavailable for ${build.spec}";

  repo = rec
    { sourceDir = ./src; includeDir = ./include;
      includeDirs =
        [ includeDir
          ./src/platform
        ];
    };
in
repo // {

  core = import ./src/core { inherit build base repo; };

  lib = {
    # these get overridden when combined with base
    base = import ./lib/mk/base.nix { inherit base repo; };
    base-common = import ./lib/mk/base-common.nix { inherit base repo; };
    syscall = import syscallPath { inherit (repo) sourceDir; } // { includeDirs = []; };

    lx_hybrid = build.library {
      name = "lx_hybrid";
      libs = with base.lib;
        [ base-common ]
        # from lib/mk/base.inc
        ++ [ syscall cxx ];
      sources =
        [ ./src/platform/lx_hybrid.cc
          (base.sourceDir + "/base/cxx/new_delete.cc")
        ] ++
        # from lib/mk/base.inc
        [ (base.sourceDir + "/base/console/log_console.cc")
          (base.sourceDir + "/base/env/env.cc")
          ./src/base/env/platform_env.cc
          (base.sourceDir + "/base/env/context_area.cc")
        ];
      includeDirs = [ ./src/base/env (base.sourceDir + "/base/env") ] ++ base.includeDirs;
    };

  };

  #test = {
    #lx_hybrid_ctors = import ./src/test/lx_hybrid_ctors { inherit build base; };
    #lx_hybrid_errno = import ./src/test/lx_hybrid_errno { inherit build base; };
  #};

  final = {
    # nothing to combine with base at the end of evaluation.
  };

}