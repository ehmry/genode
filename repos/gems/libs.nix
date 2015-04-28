{ spec, tool, callLibrary
, baseIncludes, osIncludes, gemsIncludes, ... }:

let
  callLibrary' = callLibrary
    { compileCC = tool.addIncludes (gemsIncludes ++ baseIncludes ++ osIncludes) [ ] tool.compileCC; };
in
tool.loadExpressions callLibrary' ./src/lib
