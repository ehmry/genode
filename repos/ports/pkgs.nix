/*
 * \brief  Ports packages
 * \author Emery Hemingway
 * \date   2014-09-18
 */

{ tool, callComponent }:

let

  # Append 'Src' to each attribute in ports.
  ports = builtins.listToAttrs (
    map
      (n: { name = n+"Src"; value = builtins.getAttr n ports; })
      (builtins.attrNames (import ./ports { inherit tool; }))
  );
  
  callComponent' = callComponent (
    { inherit (tool) genodeEnv compileCCRepo;
    } // ports
  );
    

  importComponent = path: callComponent' (import path);
  
in
{
  app =
    { dosbox = importComponent ./src/app/dosbox; };
}
