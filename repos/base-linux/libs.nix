/*
 * \brief  Libraries for the Linux implementation of Base.
 * \author Emery Hemingway
 * \date   2014-09-11
 */

{ tool, importBaseLibrary }:

{
  base = importBaseLibrary ./lib/mk/base.nix;
  base-common = importBaseLibrary ./lib/mk/base-common.nix;

  syscall = importBaseLibrary ./lib/mk/syscall.nix;

  #lx_hybrid = callLibrary (
  #  { base-common, syscall, cxx }:
  #  build.library {
  #    name = "lx_hybrid";
  #    libs = [ base-common syscall cxx ]; # from lib/mk/base.inc
  #    sources =
  #      [ ./src/platform/lx_hybrid.cc
  #        (baseRepo.sourceDir + "/base/cxx/new_delete.cc")
  #      # from lib/mk/base.inc
  #        (baseRepo.sourceDir + "/base/console/log_console.cc")
  #        (baseRepo.sourceDir + "/base/env/env.cc")
  #        ./src/base/env/platform_env.cc
  #        (baseRepo.sourceDir + "/base/env/context_area.cc")
  #      ];
  #    includeDirs =
  #      [ ./src/base/env
  #        ./src/platform
  #        (baseRepo.sourceDir + "/base/env")
  #      ];# ++ baseRepo.includeDirs;
  #  }
  #);

}