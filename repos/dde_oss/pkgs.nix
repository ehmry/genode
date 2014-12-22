{ tool, callComponent }:

let

  ports = import ./ports { inherit tool; };
  # Append 'Src' to each attribute in ports.
  ports' = builtins.listToAttrs (
    map
      (n: { name = n+"Src"; value = builtins.getAttr n ports; })
      (builtins.attrNames ports)
  );

  importInclude = p: import p { inherit (tool) genodeEnv; };
  
  compileCC = attrs:
    tool.compileCC (attrs // {
      systemIncludes =
       (attrs.systemIncludes or []) ++
       (importInclude ../base/include);
    });

  callComponent' = callComponent (
    { inherit compileCC; } // ports'
  );
  importComponent = path: callComponent' (import path);

in
{
  driver.audio_out = importComponent ./src/drivers/audio_out; 
}
