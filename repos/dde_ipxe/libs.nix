{ spec, tool, callLibrary }:

let

  # Append 'Src' to each attribute in ports.
  ports =
    let p = import ./ports { inherit tool; }; in
    builtins.listToAttrs (
      map
        (n: { name = n+"Src"; value = builtins.getAttr n p; })
        (builtins.attrNames p)
    );

  importLibrary = path: callLibrary ports (import path);

in {
  dde_ipxe_support = importLibrary ./src/lib/dde_ipxe/dde_support.nix;
  dde_ipxe_nic = importLibrary ./src/lib/dde_ipxe;
}
