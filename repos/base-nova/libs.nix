/*
 * \brief  Libraries for the NOVA implementation of Base.
 * \author Emery Hemingway
 * \date   2014-09-15
 */

{ tool, importBaseLibrary }:

{
  base = importBaseLibrary ./lib/mk/base.nix;
  base-common = importBaseLibrary ./lib/mk/base-common.nix;
  lock =
    { name = "lock";
      propagate.includes = [ ./src/base/lock ];
    };
}
