/*
 * \brief  Demo components
 * \author Emery Hemingway
 * \date   2014-09-15
 */

{ spec, tool, callComponent
, baseIncludes, osIncludes, demoIncludes, ... }:

let

  addIncludes =
  f: attrs:
  f (attrs // {
    includes = (attrs.includes or []) ++ demoIncludes;
    externalIncludes =
     (attrs.externalIncludes or []) ++ osIncludes ++ baseIncludes;
  });

  callComponent' = callComponent
    { compileCC = addIncludes tool.compileCC; };

in builtins.removeAttrs (tool.loadExpressions callComponent' ./src) [ "lib" ]
