/*
 * \brief  Ports libraries
 * \author Emery Hemingway
 * \date   2014-12-14
 */

{ tool, callLibrary }:

let

  ports = import ./ports { inherit tool; };
  # Append 'Src' to each attribute in ports.
  ports' = builtins.listToAttrs (
    map
      (n: { name = n+"Src"; value = builtins.getAttr n ports; })
      (builtins.attrNames ports)
  );

  importInclude = p: import p { inherit (tool) genodeEnv; };

  compileCC =
  attrs:
  tool.compileCC (attrs // {
    systemIncludes =
     (attrs.systemIncludes or []) ++
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
}
