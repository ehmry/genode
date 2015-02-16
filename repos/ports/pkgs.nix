/*
 * \brief  Ports packages
 * \author Emery Hemingway
 * \date   2014-09-18
 */

{ spec, tool, callComponent
, baseIncludes, osIncludes, portsIncludes, ... }:

let
  # Append 'Src' to each attribute in ports.
  ports =
    let p = import ./ports { inherit tool; }; in
    builtins.listToAttrs (map
      (n: { name = n+"Src"; value = builtins.getAttr n p; })
      (builtins.attrNames p)
  );

  callComponent' = callComponent
    ( ports //
      { nixSrc = /home/emery/repo/nix;
        compileCC = tool.addIncludes portsIncludes (osIncludes ++ baseIncludes) tool.compileCC;
      }
    );

in builtins.removeAttrs (tool.loadExpressions (callComponent') ./src) [ "lib" ]
