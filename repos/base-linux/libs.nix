/*
 * \brief  Libraries for the Linux implementation of Base.
 * \author Emery Hemingway
 * \date   2014-09-11
 */

{ tool, importBaseLibrary }:

{
  base = importBaseLibrary ./lib/mk/base.nix;
  base-common = importBaseLibrary ./lib/mk/base-common.nix;

  env =
    { name = "env";
      propagate.systemIncludes =
        [ (tool.filterHeaders ./src/base/env)
          (tool.filterHeaders ../base/src/base/env)
        ];
    };

  lock =
    { name = "lock";
      propagate.systemIncludes =
        [ (tool.filterHeaders ./src/base/lock)
          (tool.filterHeaders ../base/src/base/lock)
        ];
    };

  lx_hybrid = importBaseLibrary ./lib/mk/lx_hybrid.nix;

  syscall = importBaseLibrary ./lib/mk/syscall.nix;

}
