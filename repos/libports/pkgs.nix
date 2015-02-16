/*
 * \brief  Libports components
 * \author Emery Hemingway
 * \date   2014-09-21
 */

{ spec, tool, callComponent
, baseIncludes, osIncludes, libportsIncludes, ... }:

let

  # Append 'Src' to each attribute in ports.
  ports =
    let p = import ./ports { inherit tool; }; in
    builtins.listToAttrs (map
      (n: { name = n+"Src"; value = builtins.getAttr n p; })
      (builtins.attrNames p)
  );

  compileCC =
    tool.addIncludes libportsIncludes (osIncludes ++ baseIncludes) tool.compileCC;

  callComponent' = callComponent ({ inherit compileCC; } // ports);

in builtins.removeAttrs (tool.loadExpressions callComponent' ./src) [ "lib" ]