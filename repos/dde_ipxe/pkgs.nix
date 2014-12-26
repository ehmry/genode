{ tool, callComponent }:

let
  importInclude = p: import p { inherit (tool) genodeEnv; };

  compileCC = attrs:
    tool.compileCC (attrs // {
      systemIncludes =
       (attrs.systemIncludes or []) ++
       (importInclude ../base/include);
    });

  callComponent' = callComponent { inherit compileCC; };
  importComponent = path: callComponent' (import path);

in
{
  driver.nic      = importComponent ./src/drivers/nic;
  driver.nic_stat = importComponent ./src/drivers/nic_stat;
}
