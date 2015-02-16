{ spec, tool, callComponent
, baseIncludes, osIncludes, gemsIncludes, ... }:

let
  callComponent' = callComponent
    { compileCC = tool.addIncludes gemsIncludes (baseIncludes ++ osIncludes) tool.compileCC; };
in
builtins.removeAttrs (tool.loadExpressions callComponent' ./src) [ "lib" ]
