/*
 * \brief  Ports libraries
 * \author Emery Hemingway
 * \date   2014-12-14
 */

{ spec, tool, callLibrary
, baseIncludes, osIncludes, portsIncludes, ... }:

let

  ports = import ./ports { inherit tool; };
  # Append 'Src' to each attribute in ports.
  ports' = builtins.listToAttrs (
    map
      (n: { name = n+"Src"; value = builtins.getAttr n ports; })
      (builtins.attrNames ports)
  );

  compileCC =
    tool.addIncludes portsIncludes (osIncludes ++ baseIncludes) tool.compileCC;

  importLibrary = path: callLibrary
    ( ports' //
      { inherit compileCC;
        nixSrc = ../../../nix;
      }
    ) (import path);

in
{
  libc_noux = importLibrary ./src/lib/libc_noux;

  libnixexpr   = importLibrary ./src/app/nix/libexpr.nix;
  libnixformat = importLibrary ./src/app/nix/libformat.nix;
  libnixstore  = importLibrary ./src/app/nix/libstore.nix;
  libnixutil   = importLibrary ./src/app/nix/libutil.nix;

  # Native Nix libraries
  nixmain = importLibrary ./src/lib/nixmain/default.nix;

  # seoul_libc_support defined in this repo directory but imported in ../libports.
}
