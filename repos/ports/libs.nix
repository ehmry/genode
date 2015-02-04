/*
 * \brief  Ports libraries
 * \author Emery Hemingway
 * \date   2014-12-14
 */

{ spec, tool, callLibrary }:

let

  ports = import ./ports { inherit tool; };
  # Append 'Src' to each attribute in ports.
  ports' = builtins.listToAttrs (
    map
      (n: { name = n+"Src"; value = builtins.getAttr n ports; })
      (builtins.attrNames ports)
  );

  importInclude = p: import p { inherit spec; };

  compileCC =
  attrs:
  tool.compileCC (attrs // {
    includes =
     (attrs.includes or []) ++
     [ ./include ../libports/include ] ++
     (importInclude ../base/include);
  });

  callLibrary' = callLibrary (
    { inherit compileCC; } // ports'
  );

  importLibrary = path: callLibrary' (import path);

in
{
  libc_noux = importLibrary ./src/lib/libc_noux;

  #libnixexpr = importLibrary ./src/app/nix/libexpr.nix;
  #libnixformat = importLibrary ./src/app/nix/libformat.nix;
  #libnixstore = importLibrary ./src/app/nix/libstore.nix;
  #libnixutil = importLibrary ./src/app/nix/libutil.nix;

  # seoul_libc_support is imported in ../libports.
}
