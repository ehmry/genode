/*
 * \brief  OS components
 * \author Emery Hemingway
 * \date   2014-09-15
 */

{ spec, tool, callComponent
, baseIncludes, osIncludes, ... }:

let
  callComponent' =  callComponent
    { compileCC = tool.addIncludes osIncludes baseIncludes tool.compileCC; };
in
builtins.removeAttrs (tool.loadExpressions callComponent' ./src) [ "lib" ]