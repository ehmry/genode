/*
 * \brief  Ports packages
 * \author Emery Hemingway
 * \date   2014-09-18
 */

{ spec, tool, callComponent }:

let

  # Append 'Src' to each attribute in ports.
  ports =
    let p = import ./ports { inherit tool; }; in
    builtins.listToAttrs (map
      (n: { name = n+"Src"; value = builtins.getAttr n p; })
      (builtins.attrNames p)
  );

  importComponent = path: callComponent ports (import path);

in
{
  app =
    { dosbox = importComponent ./src/app/dosbox;
      seoul  = importComponent ./src/app/seoul;
    };

  noux.minimal = importComponent ./src/noux/minimal;
  noux.net     = importComponent ./src/noux/net;

  test.vmm_utils = importComponent ./src/test/vmm_utils;
}
