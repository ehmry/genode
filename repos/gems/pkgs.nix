/*
 * \brief  Gem components
 * \author Emery Hemingway
 * \date   2014-12-12
 */

{ spec, tool, callComponent }:

let

  importInclude = p: import p { inherit spec; };

  compileCC = attrs:
    tool.compileCC (attrs // {
      systemIncludes =
       (attrs.systemIncludes or []) ++
       (importInclude ../base/include) ++
       [ ./include ];
    });

  callComponent' = callComponent {
    inherit compileCC;
  };
  importComponent = path: callComponent' (import path);

in
{
  server.terminal = importComponent ./src/server/terminal;
}
