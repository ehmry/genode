{ spec, tool, callLibrary }:

let

  addIncludes =
  f: attrs:
  f (attrs // {
    systemIncludes =
     (attrs.systemIncludes or []) ++
     [ ./include ] ++
     (import ../base/include { inherit (tool) genodeEnv; });
  });

  ports = import ./ports { inherit tool; };
  # Append 'Src' to each attribute in ports.
  ports' = builtins.listToAttrs (
    map
      (n: { name = n+"Src"; value = builtins.getAttr n ports; })
      (builtins.attrNames ports)
  );

  callLibrary' = callLibrary (
    { inherit (tool) genodeEnv;
      compileS  = addIncludes tool.compileS;
      compileC  = addIncludes tool.compileC;
      compileCC = addIncludes tool.compileCC;
    } // ports'
  );
  importLibrary = path: callLibrary' (import path);

in {
  dde_ipxe_support = importLibrary ./src/lib/dde_ipxe/dde_support.nix;
  dde_ipxe_nic = importLibrary ./src/lib/dde_ipxe;
}
