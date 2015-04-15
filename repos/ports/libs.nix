/*
 * \brief  Ports libraries
 * \author Emery Hemingway
 * \date   2014-12-14
 */

{ spec, tool, callLibrary
, baseIncludes, osIncludes, portsIncludes, ... }:

let
  ports =
    let p = import ./ports { inherit tool; }; in
    builtins.listToAttrs (map
      (n: { name = n+"Src"; value = builtins.getAttr n p; })
      (builtins.attrNames p)
    );

  callLibrary' = callLibrary
    ({ compileCC = tool.addIncludes portsIncludes (baseIncludes ++ osIncludes) tool.compileCC;
    } // ports);

in
  # seoul_libc_support defined in this repo directory but imported in ../libports.
tool.loadExpressions callLibrary' ./src/lib
